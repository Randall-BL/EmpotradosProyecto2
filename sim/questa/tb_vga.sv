// Testbench autoverificable del video VGA (HW-09, NV-11, PR-05).
//
// El Nios V ejecuta sw/niosv/tests/test_vga: espera el borrado de arranque del
// Character Buffer y dibuja la pantalla del reproductor (80 x 60) con los datos de
// tests/test_screen.c. El testbench verifica:
//   1. Texto: en el stream de pixeles que sale del Character Buffer (el SOP marca el
//      pixel (0,0), asi que cada pixel tiene posicion exacta), cada celda de las
//      filas 9 a 12 tiene pixeles encendidos si y solo si la referencia (salida de
//      tests/test_screen -v) tiene un caracter distinto de espacio. Valida el
//      direccionamiento (x, y) del texto desde el procesador hasta los pixeles.
//   2. Temporizacion en los pines: linea de 800 pixeles (32 us a 25 MHz), pulso de
//      HS de 96 pixeles, 640 pixeles activos por linea, y que lleguen pixeles
//      encendidos a VGA_R/G/B (el camino FIFO de doble reloj -> VGA funciona).
//
// Artefactos de simulacion (no del diseno):
// - La ROM de la fuente se inicializa con un .mif de 8 valores por linea, que el
//   modelo altsyncram no lee bien; sim-tb.sh lo expande (scripts/expand-mif.py).
// - La FIFO de doble reloj del IP declara
// intended_device_family = "Cyclone II" y modo show-ahead; con eso el modelo dcfifo
// de altera_mf deja su salida en X hasta la primera lectura, en vez de mostrar la
// primera palabra como hace el hardware. El controlador VGA calcula
// ready = valid & ~startofpacket, que queda en X y traba el stream. Aqui el
// startofpacket que ve el VGA se fuerza a "1 solo si vale 1" (X -> 0).
//
// Uso: scripts/sim-tb.sh vga   (~25 ms simulados: el borrado de arranque y el
//      primer cuadro van al ritmo del video)
`timescale 1ns / 1ps

module tb_vga;
    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;

    wire [9:0] led;
    wire vga_clk, hs, vs, blank_n, sync_n;
    wire [7:0] r, g, b;
    wire sda;
    pullup (sda);

    audio_soc dut (
        .clk_clk        (clk),
        .reset_reset_n  (rst_n),
        .key_export     (4'hF),
        .sw_export      (10'h000),
        .led_export     (led),
        .audio_clk_clk  (),
        .audio_BCLK     (1'b0),
        .audio_DACLRCK  (1'b0),
        .audio_DACDAT   (),
        .av_config_SCLK (),
        .av_config_SDAT (sda),
        .adc_sclk       (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK        (vga_clk), .vga_HS (hs), .vga_VS (vs), .vga_BLANK (blank_n),
        .vga_SYNC       (sync_n), .vga_R (r), .vga_G (g), .vga_B (b)
    );

    // Ver el encabezado: X del modelo dcfifo -> 0 en el startofpacket que ve el VGA.
    initial force dut.vga.startofpacket = (dut.video_fifo.stream_out_startofpacket === 1'b1);

    // Referencia: filas 9..12 de la pantalla (tests/test_screen -v).
    localparam int FIRST_ROW = 9, NROWS = 4, W = 640;
    string expect_rows [NROWS] = '{
        "| Volumen : [##########----------]  50%                                        |",
        "|------------------------------------------------------------------------------|",
        "| KEY0 Play/Pausa  KEY1 Stop  KEY2 Anterior  KEY3 Siguiente   SW: filtro       |",
        "+------------------------------------------------------------------------------+"
    };

    int errors = 0;
    always @(led) $display("[%0t] LEDR = 0x%03h", $time, led);

    // ---- 1. Texto en el stream del Character Buffer ----
    int pix = -1;                        // indice del pixel en el cuadro (desde el SOP)
    int frame = 0;
    int lit [NROWS][80];
    realtime t_rows_start = 0;           // cuando salio la primera linea de las filas
    bit rows_done = 0;                   // el cuadro actual ya paso las filas revisadas

    always @(posedge dut.char_buf.clk)
        if (dut.char_buf.stream_valid && dut.char_buf.stream_ready) begin
            if (dut.char_buf.stream_startofpacket) begin
                pix = 0;
                frame++;
                rows_done = 0;
                foreach (lit[i, j]) lit[i][j] = 0;
            end else if (pix >= 0)
                pix++;
            if (pix >= 0) begin
                automatic int y = pix / W, x = pix % W;
                automatic int row = y / 8, col = x / 8;
                if (pix == FIRST_ROW * 8 * W) t_rows_start = $realtime;
                if (row >= FIRST_ROW && row < FIRST_ROW + NROWS && dut.char_buf.stream_data != 0)
                    lit[row - FIRST_ROW][col]++;
                if (pix == (FIRST_ROW + NROWS) * 8 * W) rows_done = 1;
            end
        end

    // ---- 2. Temporizacion y pixeles en los pines ----
    realtime t_fall [$];
    realtime t_rise_last;
    int px = 0, line = 0, bad_width = 0, good_lines = 0, pins_lit = 0;
    bit prev_blank = 0;
    always @(negedge hs) t_fall.push_back($realtime);
    always @(posedge hs) t_rise_last = $realtime;
    always @(posedge vga_clk) begin
        if (blank_n && !prev_blank) px = 0;
        if (!blank_n && prev_blank) begin
            if (line > 0) begin
                if (px == W) good_lines++; else bad_width++;
            end
            line++;
        end
        if (blank_n) begin
            px++;
            if (r != 0 || g != 0 || b != 0) pins_lit++;
        end
        prev_blank = blank_n;
    end

    realtime t_done;

    initial begin
        $display("TB_VGA: inicio");
        #200 rst_n = 1'b1;

        fork
            wait (led == 10'h0B0 || led[9:4] == 6'h0C);
            begin #40ms; $error("timeout esperando al software (LEDR = 0x%03h)", led); errors++; end
        join_any
        disable fork;
        t_done = $realtime;
        if (led !== 10'h0B0) begin
            $error("el software no termino bien (LEDR = 0x%03h)", led);
            errors++;
        end else
            $display("[%0t] OK    pantalla dibujada; resolucion reportada 80x60", $time);

        // Primer cuadro del stream cuyas filas salieron despues de terminar el dibujo.
        fork
            begin
                do begin
                    wait (rows_done);
                    if (t_rows_start < t_done) wait (!rows_done);
                end while (t_rows_start < t_done);
            end
            begin #40ms; $error("timeout esperando un cuadro con la pantalla dibujada"); errors++; end
        join_any
        disable fork;
        $display("[%0t] cuadro %0d del stream: filas %0d-%0d desde %.2f ms", $time, frame,
                 FIRST_ROW, FIRST_ROW + NROWS - 1, t_rows_start / 1.0e6);

        for (int rr = 0; rr < NROWS; rr++) begin
            automatic int bad = 0;
            for (int c = 0; c < 80; c++) begin
                automatic bit want = (expect_rows[rr][c] != " ");
                automatic bit got  = (lit[rr][c] > 0);
                if (want != got) begin
                    if (bad < 3)
                        $error("fila %0d col %0d ('%s'): %0d pixeles encendidos",
                               FIRST_ROW + rr, c, expect_rows[rr].substr(c, c), lit[rr][c]);
                    bad++;
                end
            end
            if (bad == 0) $display("OK    fila %0d: las 80 celdas coinciden con la referencia", FIRST_ROW + rr);
            errors += bad;
        end

        // Temporizacion en los pines.
        begin
            automatic int n = t_fall.size();
            automatic realtime period = t_fall[n-1] - t_fall[n-2];
            automatic realtime width  = (t_rise_last > t_fall[n-1]) ? t_rise_last - t_fall[n-1]
                                                                    : t_rise_last - t_fall[n-2];
            if (period < 31.9us || period > 32.1us) begin
                $error("periodo de linea %.3f us (se esperaban 32.000 us)", period / 1000.0); errors++;
            end else $display("OK    periodo de linea %.3f us (800 pixeles a 25 MHz)", period / 1000.0);
            if (width < 3.80us || width > 3.88us) begin
                $error("pulso de HS %.3f us (se esperaban 3.840 us)", width / 1000.0); errors++;
            end else $display("OK    pulso de HS %.3f us (96 pixeles)", width / 1000.0);
            if (bad_width != 0) begin
                $error("%0d lineas sin 640 pixeles activos", bad_width); errors++;
            end else $display("OK    %0d lineas con 640 pixeles activos", good_lines);
            if (pins_lit == 0) begin
                $error("ningun pixel encendido llego a VGA_R/G/B"); errors++;
            end else $display("OK    %0d pixeles encendidos llegaron a los pines VGA", pins_lit);
        end

        if (errors == 0) $display("TB_VGA: PASS");
        else             $display("TB_VGA: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
