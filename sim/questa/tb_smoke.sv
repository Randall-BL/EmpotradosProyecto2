// Testbench de la prueba de humo (sw/niosv/src/main.c) para scripts/sim-soc.sh.
//
// Reemplaza al audio_soc_tb que genera Platform Designer: con varios conduits
// exportados, sus BFM (altera_conduit_bfm_000x) se resuelven al modulo
// equivocado al elaborar y la simulacion no carga. Aqui el SoC se conecta a
// senales simples. La salida del printf aparece en el transcript (modelo de
// simulacion de la JTAG UART).
`timescale 1ns / 1ps

module tb_smoke;
    logic       clk   = 1'b0;
    logic       rst_n = 1'b0;
    logic [3:0] key   = 4'hF;
    logic [9:0] sw    = 10'h00A;     // el programa imprime PIO_IN = 0x0000000a
    wire  [9:0] led;
    wire        i2c_sdat;

    always #10 clk = ~clk;
    initial #200 rst_n = 1'b1;

    pullup (i2c_sdat);

    audio_soc dut (
        .clk_clk        (clk),
        .reset_reset_n  (rst_n),
        .key_export     (key),
        .sw_export      (sw),
        .led_export     (led),
        .audio_clk_clk  (),
        .audio_BCLK     (1'b0),
        .audio_DACLRCK  (1'b0),
        .audio_DACDAT   (),
        .av_config_SCLK (),
        .av_config_SDAT (i2c_sdat),
        .adc_sclk       (),
        .adc_cs_n       (),
        .adc_din        (),
        .adc_dout       (1'b0),
        .vga_CLK        (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R          (), .vga_G  (), .vga_B  ()
    );
endmodule
