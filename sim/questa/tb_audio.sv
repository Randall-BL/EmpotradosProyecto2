// Testbench autoverificable del camino de audio (HW-07, NV-06, PR-05).
//
// El Nios V ejecuta sw/niosv/tests/test_audio: envia el par n con
// izquierda = n * 0x0123 y derecha = ~izquierda, primero por sondeo y luego por
// la IRQ de escritura del IP de Audio. wm8731_model hace de codec maestro a
// 48 kHz y deserializa AUD_DACDAT.
//
// Verifica, durante FRAMES pares consecutivos desde el primer par no nulo:
//   - cada canal llega con sus 16 bits en orden (Left Justified, MSB primero);
//   - derecha == ~izquierda (los canales no se cruzan ni se desfasan);
//   - izquierda avanza 0x0123 por par: ninguna muestra se pierde ni se repite, o
//     sea, la FIFO nunca se vacia (reproduccion sin cortes, §3-F p.4);
//   - el cambio de sondeo a IRQ (LEDR 0x0A1 -> 0x0A2) ocurre dentro de la ventana.
//
// Uso: scripts/sim-tb.sh audio
`timescale 1ns / 1ps

module tb_audio;
    localparam int FRAMES = 240;          // 5 ms de audio a 48 kHz
    localparam logic [15:0] STEP = 16'h0123;

    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;

    wire [9:0] led;
    wire bclk, daclrck, dacdat, scl, sda;
    wire [15:0] left, right;
    wire frame_valid;
    pullup (sda);

    audio_soc dut (
        .clk_clk        (clk),
        .reset_reset_n  (rst_n),
        .key_export     (4'hF),
        .sw_export      (10'h000),
        .led_export     (led),
        .audio_clk_clk  (),
        .audio_BCLK     (bclk),
        .audio_DACLRCK  (daclrck),
        .audio_DACDAT   (dacdat),
        .av_config_SCLK (scl),
        .av_config_SDAT (sda),
        .adc_sclk       (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK        (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R          (), .vga_G  (), .vga_B  ()
    );

    wm8731_model codec (
        .enable (rst_n), .bclk (bclk), .daclrck (daclrck), .dacdat (dacdat),
        .left (left), .right (right), .frame_valid (frame_valid),
        .scl (scl), .sda (sda)
    );

    int errors = 0, frames = 0;
    bit started = 0, saw_irq_stage = 0;
    logic [15:0] expect_left;

    always @(posedge frame_valid) begin
        if (!started && left != 16'h0000) begin
            started = 1;
            expect_left = left;
            $display("[%0t] primer par recibido: L = 0x%04h R = 0x%04h", $time, left, right);
        end
        if (started && frames < FRAMES) begin
            if (left !== expect_left) begin
                if (errors < 10)
                    $error("par %0d: L = 0x%04h, se esperaba 0x%04h", frames, left, expect_left);
                errors++;
            end
            if (right !== ~left) begin
                if (errors < 10)
                    $error("par %0d: R = 0x%04h, se esperaba ~L = 0x%04h", frames, right, ~left);
                errors++;
            end
            if (led == 10'h0A2) saw_irq_stage = 1;
            expect_left = left + STEP;
            frames++;
        end
    end

    initial begin
        $display("TB_AUDIO: inicio");
        #200 rst_n = 1'b1;
        fork
            wait (frames == FRAMES);
            begin #15ms; $error("timeout: solo %0d de %0d pares", frames, FRAMES); errors++; end
        join_any
        disable fork;
        $display("Pares verificados: %0d (%.2f ms de audio a 48 kHz)", frames, frames / 48.0);
        if (!saw_irq_stage) begin
            $error("el programa no paso al relleno por IRQ dentro de la ventana");
            errors++;
        end else
            $display("Relleno por sondeo y por IRQ de escritura: sin huecos");
        if (errors == 0) $display("TB_AUDIO: PASS");
        else             $display("TB_AUDIO: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
