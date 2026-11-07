// Top-level del reproductor de audio para la DE1-SoC (HW-19).
//
// Instancia el SoC de Platform Designer (audio_soc, generado desde
// hw/soc/build_system.tcl) y lo conecta a los pines de la tarjeta. Los nombres de
// los puertos son los del DE1_SOC_golden_top de Terasic, de donde salen tambien
// los pines de audio_player.qsf.
//
// Reset: los cuatro KEY son controles del reproductor, asi que no queda boton
// para reset. Se usa un reset de encendido interno: el FPGA inicia sus registros
// en 0 al configurarse y el contador mantiene el SoC en reset 2^15 ciclos
// (655 us), hasta que los PLL de audio y video se estabilizan.
module DE1_SoC_top (
    input  wire        CLOCK_50,

    input  wire [3:0]  KEY,
    input  wire [9:0]  SW,
    output wire [9:0]  LEDR,
    output wire [6:0]  HEX0,
    output wire [6:0]  HEX1,
    output wire [6:0]  HEX2,
    output wire [6:0]  HEX3,
    output wire [6:0]  HEX4,
    output wire [6:0]  HEX5,

    // Codec WM8731 (maestro: genera BCLK y LRCK)
    input  wire        AUD_ADCDAT,
    inout  wire        AUD_ADCLRCK,
    inout  wire        AUD_BCLK,
    output wire        AUD_DACDAT,
    inout  wire        AUD_DACLRCK,
    output wire        AUD_XCK,
    output wire        FPGA_I2C_SCLK,
    inout  wire        FPGA_I2C_SDAT,

    // ADC del potenciometro (AD7928 o LTC2308 segun la revision de la placa)
    output wire        ADC_CONVST,
    output wire        ADC_DIN,
    input  wire        ADC_DOUT,
    output wire        ADC_SCLK,

    // VGA (DAC ADV7123)
    output wire [7:0]  VGA_R,
    output wire [7:0]  VGA_G,
    output wire [7:0]  VGA_B,
    output wire        VGA_CLK,
    output wire        VGA_HS,
    output wire        VGA_VS,
    output wire        VGA_BLANK_N,
    output wire        VGA_SYNC_N
);

    // ---- Reset de encendido ----
    reg [15:0] por_count = 16'd0;
    always @(posedge CLOCK_50) begin
        if (!por_count[15])
            por_count <= por_count + 16'd1;
    end
    wire reset_n = por_count[15];

    // ---- SoC ----
    audio_soc u_soc (
        .clk_clk        (CLOCK_50),
        .reset_reset_n  (reset_n),

        .key_export     (KEY),
        .sw_export      (SW),
        .led_export     (LEDR),

        .audio_clk_clk  (AUD_XCK),
        .audio_BCLK     (AUD_BCLK),
        .audio_DACLRCK  (AUD_DACLRCK),
        .audio_DACDAT   (AUD_DACDAT),
        .av_config_SCLK (FPGA_I2C_SCLK),
        .av_config_SDAT (FPGA_I2C_SDAT),

        .adc_sclk       (ADC_SCLK),
        .adc_cs_n       (ADC_CONVST),
        .adc_din        (ADC_DIN),
        .adc_dout       (ADC_DOUT),

        .vga_CLK        (VGA_CLK),
        .vga_HS         (VGA_HS),
        .vga_VS         (VGA_VS),
        .vga_BLANK      (VGA_BLANK_N),
        .vga_SYNC       (VGA_SYNC_N),
        .vga_R          (VGA_R),
        .vga_G          (VGA_G),
        .vga_B          (VGA_B),

        // Displays: tiempo MM:SS y cancion (seg7_time, HW-10)
        .hex_hex0       (HEX0),
        .hex_hex1       (HEX1),
        .hex_hex2       (HEX2),
        .hex_hex3       (HEX3),
        .hex_hex4       (HEX4),
        .hex_hex5       (HEX5)
    );

    // El codec es maestro: BCLK y DACLRCK son entradas para la FPGA. La entrada de
    // grabacion (ADC del codec) no se usa: el IP de Audio es solo de salida.
    assign AUD_ADCLRCK = 1'bz;

endmodule
