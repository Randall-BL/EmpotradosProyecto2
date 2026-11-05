// Testbench autoverificable del ADC del volumen (HW-08, NV-08, PR-05).
//
// El Nios V ejecuta sw/niosv/tests/test_adc: lee CH0 del DE1-SoC ADC Controller
// y lo copia a LEDR (>> 2). adc_model hace del chip de la placa: LTC = 0 es el
// AD7928 y LTC = 1 el LTC2308 (el IP detecta cual hay). El tb mueve el
// potenciometro (canal 0) por varios valores, con valores distintos en los otros
// canales, y verifica que LEDR siga al canal 0 en ambos chips.
//
// Uso: scripts/sim-tb.sh adc                  (AD7928)
//      ELAB_OPTS=-gLTC=1 scripts/sim-tb.sh adc   (LTC2308)
`timescale 1ns / 1ps

module tb_adc #(
    parameter bit LTC = 0
);
    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;

    wire [9:0] led;
    wire adc_sclk, adc_cs_n, adc_din, adc_dout;
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
        .adc_sclk       (adc_sclk),
        .adc_cs_n       (adc_cs_n),
        .adc_din        (adc_din),
        .adc_dout       (adc_dout),
        .vga_CLK        (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R          (), .vga_G  (), .vga_B  ()
    );

    adc_model #(.LTC(LTC)) chip (
        .cs_n (adc_cs_n), .sclk (adc_sclk), .din (adc_din), .dout (adc_dout)
    );

    int errors = 0;
    localparam logic [11:0] POT [5] = '{12'h000, 12'hFFF, 12'h800, 12'h3A5, 12'hC3C};

    initial begin
        $display("TB_ADC: inicio, chip = %s", LTC ? "LTC2308" : "AD7928");
        // Otros canales con valores que no se confunden con el potenciometro.
        chip.ch_value[1] = 12'h555;
        for (int i = 2; i < 8; i++) chip.ch_value[i] = 12'hAAA;
        chip.ch_value[0] = 12'h123;
        #200 rst_n = 1'b1;

        foreach (POT[i]) begin
            chip.ch_value[0] = POT[i];
            fork
                wait (led === POT[i][11:2]);
                #300us;
            join_any
            disable fork;
            if (led !== POT[i][11:2]) begin
                $error("pot = 0x%03h: LEDR = 0x%03h, se esperaba 0x%03h", POT[i], led, POT[i][11:2]);
                errors++;
            end else
                $display("[%0t] OK    pot = 0x%03h -> CH0 >> 2 = 0x%03h en LEDR", $time, POT[i], led);
            #20us;
        end
        $display("Conversiones del chip: %0d", chip.conversions);

        if (errors == 0) $display("TB_ADC: PASS");
        else             $display("TB_ADC: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
