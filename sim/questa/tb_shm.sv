// Testbench autoverificable de la memoria compartida vista desde el Nios V (HW-06).
// El Nios V ejecuta sw/niosv/tests/test_shm (bus de datos, bus de direcciones en
// los 64 KB, March C- en los extremos, no solapamiento con la RAM y accesos de
// 8/16 bits) y reporta el resultado en LEDR.
//
// Uso: scripts/sim-tb.sh shm
`timescale 1ns / 1ps

module tb_shm;
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
        $display("TB_SHM: inicio");
        #200 rst_n = 1'b1;
        fork
            wait (led == 10'h2A0 || led[9:4] == 6'h2E);
            #20ms;
        join_any
        disable fork;
        case (led)
            10'h2A0: begin
                $display("[%0t] OK    shm: bus de datos, bus de direcciones (64 KB), March C-, sin solape con la RAM, accesos de 8/16 bits", $time);
                $display("TB_SHM: PASS");
            end
            default: begin
                $error("LEDR = 0x%03h (0x2Ex = fallo en la prueba x; 0x2A1 = no termino)", led);
                $display("TB_SHM: FAIL");
            end
        endcase
        $finish;
    end
endmodule
