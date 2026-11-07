// Corre sw/niosv/tests/test_ciclos en el SoC completo y deja en el transcript las
// lineas "CICLOS <medida> <ciclos>" que imprime el Nios V por la JTAG UART
// (CD-04, CD-05, CD-12, RP-04). Pasa si el programa termina (LEDR = 0x0E0).
//
// Uso: scripts/sim-tb.sh ciclos ; grep CICLOS build/sim/tb_ciclos.log
`timescale 1ns / 1ps

module tb_ciclos;
    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;
    wire [9:0] led;
    wire sda;
    pullup (sda);

    audio_soc dut (
        .clk_clk (clk), .reset_reset_n (rst_n),
        .key_export (4'hF), .sw_export (10'h000), .led_export (led),
        .audio_clk_clk (), .audio_BCLK (1'b0), .audio_DACLRCK (1'b0), .audio_DACDAT (),
        .av_config_SCLK (), .av_config_SDAT (sda),
        .adc_sclk (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R (), .vga_G (), .vga_B ()
    );

    initial begin
        $display("TB_CICLOS: inicio");
        #200 rst_n = 1'b1;
        fork forever @(led) $display("[%0t] LEDR = 0x%03h", $time, led); join_none
        fork
            wait (led == 10'h0E0);
            #40ms;
        join_any
        disable fork;
        #3ms;                                   // la JTAG UART simulada termina de imprimir
        if (led == 10'h0E0) $display("TB_CICLOS: PASS");
        else                $display("TB_CICLOS: FAIL (LEDR = 0x%03h)", led);
        $finish;
    end
endmodule
