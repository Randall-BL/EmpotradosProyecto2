// Verificacion aislada del modulo propio filtro_audio (HW-16, PR-02; equivale a DK-11 y
// DK-12 en Questa). Se ejercita SOLO por su interfaz Avalon-MM, como lo hara el Nios V,
// contra los vectores dorados de sim/vectors/ (scripts/design_fir.py, modelo bit a bit).
//
//  Registros (DK-12): valores de reset, lectura/escritura de cada registro, limites de
//    NTAPS, carga y relectura de coeficientes con autoincremento, SAT pegajoso con W1C,
//    CLEAR, espera (waitrequest) mientras el filtro trabaja.
//  Datos (DK-11): los 8 casos dorados (impulso, escalon, senos, saturacion, banco 1,
//    bypass con ganancia, 16 taps, ganancia alta), comparados bit a bit, y el bit SAT
//    contra lo que predice el modelo. Mide la latencia por muestra.
//
// Uso: scripts/sim-rtl.sh filtro_audio
`timescale 1ns / 1ps

module tb_filtro_audio;
    logic clk = 1'b0, reset = 1'b1;
    always #10 clk = ~clk;                       // 50 MHz

    logic [2:0]  address = '0;
    logic        read = 1'b0, write = 1'b0;
    logic [31:0] writedata = '0;
    wire  [31:0] readdata;
    wire         waitrequest;

    string vec_dir;
    filtro_audio dut (
        .clk (clk), .reset (reset),
        .avs_address (address), .avs_read (read), .avs_write (write),
        .avs_writedata (writedata), .avs_readdata (readdata), .avs_waitrequest (waitrequest)
    );

    localparam CTRL = 0, STATUS = 1, NTAPS = 2, GAIN = 3, CADDR = 4, CDATA = 5, DIN = 6, DOUT = 7;
    int errors = 0, checks = 0;

    // ---- Maestro Avalon-MM (respeta waitrequest) ----
    task automatic avs_wr(input int a, input logic [31:0] d);
        @(posedge clk);
        address <= a; writedata <= d; write <= 1'b1;
        do @(posedge clk); while (waitrequest);
        write <= 1'b0;
    endtask

    task automatic avs_rd(input int a, output logic [31:0] d);
        @(posedge clk);
        address <= a; read <= 1'b1;
        do @(posedge clk); while (waitrequest);
        d = readdata;
        read <= 1'b0;
    endtask

    // Tras el reset y tras CLEAR el nucleo pasa 128 ciclos poniendo la linea de
    // retardo en cero (BUSY = 1): se espera a que termine antes de comparar STATUS.
    task automatic wait_idle();
        logic [31:0] d;
        do avs_rd(STATUS, d); while (d[1]);
    endtask

    task automatic expect_eq(input string what, input logic [31:0] got, input logic [31:0] exp);
        checks++;
        if (got !== exp) begin
            errors++;
            $error("%s: 0x%08h, se esperaba 0x%08h", what, got, exp);
        end
    endtask

    // ---- Registros ----
    task automatic test_registros();
        logic [31:0] d;
        int err0 = errors;
        avs_rd(STATUS, d); expect_eq("STATUS justo tras el reset (BUSY: borrando)", d, 32'h0100_8002);
        wait_idle();
        avs_rd(CTRL, d);   expect_eq("reset CTRL (bypass)", d, 32'h1);
        avs_rd(STATUS, d); expect_eq("reset STATUS (version, max taps, no listo)", d, 32'h0100_8000);
        avs_rd(NTAPS, d);  expect_eq("reset NTAPS", d, 127);
        avs_rd(GAIN, d);   expect_eq("reset GAIN (1.0)", d, 32'h8000);
        avs_rd(CADDR, d);  expect_eq("reset COEF_ADDR", d, 0);

        avs_wr(CTRL, 32'h2);  avs_rd(CTRL, d);  expect_eq("CTRL banco 1, sin bypass", d, 32'h2);
        avs_wr(CTRL, 32'h7);  avs_rd(CTRL, d);  expect_eq("CTRL.CLEAR se lee 0", d, 32'h3);
        avs_wr(NTAPS, 0);     avs_rd(NTAPS, d); expect_eq("NTAPS 0 -> 1", d, 1);
        avs_wr(NTAPS, 200);   avs_rd(NTAPS, d); expect_eq("NTAPS 200 -> 128", d, 128);
        avs_wr(NTAPS, 64);    avs_rd(NTAPS, d); expect_eq("NTAPS 64", d, 64);
        avs_wr(GAIN, 32'hFFFF_ABCD); avs_rd(GAIN, d); expect_eq("GAIN solo 16 bits", d, 32'hABCD);
        wait_idle();
        avs_wr(STATUS, 32'hFFFF_FFFF); avs_rd(STATUS, d); expect_eq("STATUS solo lectura", d, 32'h0100_8000);

        // Coeficientes: escritura con autoincremento, vuelta dentro del banco, relectura
        avs_wr(CADDR, 8'hFE);                     // banco 1, indice 126
        avs_wr(CDATA, 32'h0000_1234);
        avs_wr(CDATA, 32'h0000_8001);
        avs_wr(CDATA, 32'h0000_7FFF);             // da la vuelta al indice 0 del banco 1
        avs_rd(CADDR, d); expect_eq("COEF_ADDR autoincremento con vuelta", d, 8'h81);
        avs_wr(CADDR, 8'hFE); avs_rd(CDATA, d); expect_eq("coef[1][126]", d, 32'h0000_1234);
        avs_wr(CADDR, 8'hFF); avs_rd(CDATA, d); expect_eq("coef[1][127] negativo", d, 32'hFFFF_8001);
        avs_wr(CADDR, 8'h80); avs_rd(CDATA, d); expect_eq("coef[1][0]", d, 32'h0000_7FFF);
        if (errors == err0) $display("OK    registros: reset, R/W, limites, coeficientes (%0d comprobaciones)", checks);
    endtask

    // ---- Coeficientes de diseno (los mismos que usan los vectores) ----
    task automatic cargar_coeficientes();
        logic [15:0] h [0:255];
        $readmemh({vec_dir, "/../../hw/rtl/fir_coefs.hex"}, h);
        avs_wr(CADDR, 0);
        for (int i = 0; i < 256; i++) begin
            if (i == 128) avs_wr(CADDR, 8'h80);
            avs_wr(CDATA, {16'd0, h[i]});
        end
    endtask

    // ---- Un caso dorado ----
    int max_lat = 0;
    task automatic caso(input string name, input bit expect_sat);
        int fd, bank, ntaps, gain, bypass, n, bad = 0, lat;
        logic [31:0] in, out, got, st;
        fd = $fopen({vec_dir, "/fir_", name, ".txt"}, "r");
        if (fd == 0) begin $error("no se pudo abrir fir_%s.txt", name); errors++; return; end
        void'($fscanf(fd, "%d %d %d %d %d", bank, ntaps, gain, bypass, n));
        avs_wr(NTAPS, ntaps);
        avs_wr(GAIN, gain);
        avs_wr(CTRL, {29'd0, 1'b1, bank[0], bypass[0]});   // CLEAR + banco + bypass
        avs_wr(STATUS, 32'h4);                              // borra SAT
        for (int i = 0; i < n; i++) begin
            realtime t0;
            void'($fscanf(fd, "%h %h", in, out));
            avs_wr(DIN, in);
            t0 = $realtime;
            avs_rd(DOUT, got);                              // espera por waitrequest
            lat = int'(($realtime - t0) / 20.0);
            if (lat > max_lat) max_lat = lat;
            checks++;
            if (got !== out) begin
                if (bad < 3) $error("fir_%s muestra %0d: entrada %08h salida %08h, se esperaba %08h",
                                    name, i, in, got, out);
                bad++;
            end
        end
        $fclose(fd);
        avs_rd(STATUS, st);
        checks++;
        if (st[2] !== expect_sat) begin
            $error("fir_%s: SAT = %0d, el modelo dice %0d", name, st[2], expect_sat); bad++;
        end
        errors += bad;
        if (bad == 0)
            $display("OK    fir_%-13s %3d muestras iguales bit a bit (banco %0d, %3d taps, ganancia 0x%04h, bypass %0d), SAT = %0d",
                     name, n, bank, ntaps, gain, bypass, st[2]);
    endtask

    initial begin
        if (!$value$plusargs("VEC=%s", vec_dir)) vec_dir = "../../sim/vectors";
        $display("TB_FILTRO_AUDIO: inicio (vectores en %s)", vec_dir);
        repeat (5) @(posedge clk);
        reset <= 1'b0;
        test_registros();
        cargar_coeficientes();
        caso("impulso", 0);
        caso("escalon", 0);
        caso("senos", 0);
        caso("saturacion", 1);
        caso("ruido_banco1", 0);
        caso("bypass_gain", 0);
        caso("taps16", 0);
        caso("ganancia_alta", 1);
        $display("Latencia maxima escritura de DATA_IN -> lectura de DATA_OUT: %0d ciclos (%.2f us a 50 MHz)",
                 max_lat, max_lat * 0.02);
        $display("Comprobaciones: %0d", checks);
        if (errors == 0) $display("TB_FILTRO_AUDIO: PASS");
        else             $display("TB_FILTRO_AUDIO: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
