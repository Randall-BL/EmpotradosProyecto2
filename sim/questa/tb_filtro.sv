// Co-simulacion HW/SW del modulo propio dentro del SoC (PR-04): el Nios V ejecuta
// sw/niosv/tests/test_filtro (driver, autoprueba contra el modelo en C, ciclos por par
// y displays). El tb espera el resultado en LEDR, copia las lineas "FILTRO ..." de la
// JTAG UART y decodifica HEX5..HEX0, que deben mostrar "03 02:05".
//
// Uso: scripts/sim-tb.sh filtro
`timescale 1ns / 1ps

module tb_filtro;
    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;
    wire [9:0] led;
    wire [6:0] hex [6];
    wire sda;
    pullup (sda);

    audio_soc dut (
        .clk_clk (clk), .reset_reset_n (rst_n),
        .key_export (4'hF), .sw_export (10'h000), .led_export (led),
        .audio_clk_clk (), .audio_BCLK (1'b0), .audio_DACLRCK (1'b0), .audio_DACDAT (),
        .av_config_SCLK (), .av_config_SDAT (sda),
        .adc_sclk (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R (), .vga_G (), .vga_B (),
        .hex_hex0 (hex[0]), .hex_hex1 (hex[1]), .hex_hex2 (hex[2]),
        .hex_hex3 (hex[3]), .hex_hex4 (hex[4]), .hex_hex5 (hex[5])
    );

    function automatic string digit(input logic [6:0] s);
        case (s)
        7'b1000000: return "0";  7'b1111001: return "1";  7'b0100100: return "2";
        7'b0110000: return "3";  7'b0011001: return "4";  7'b0010010: return "5";
        7'b0000010: return "6";  7'b1111000: return "7";  7'b0000000: return "8";
        7'b0010000: return "9";  7'b1111111: return " ";  default:    return "?";
        endcase
    endfunction

    int errors = 0;
    initial begin
        string shown;
        $display("TB_FILTRO: inicio");
        fork forever @(led) $display("[%0t] LEDR = 0x%03h", $time, led); join_none
        #200 rst_n = 1'b1;
        fork
            wait (led == 10'h0D0 || led == 10'h0DF);
            begin #30ms; $error("timeout (LEDR = 0x%03h)", led); errors++; end
        join_any
        disable fork;
        #3ms;                                   // la JTAG UART termina de imprimir
        if (led !== 10'h0D0) begin
            $error("el programa reporto un fallo (LEDR = 0x%03h)", led); errors++;
        end else
            $display("OK    autoprueba: hardware igual bit a bit al modelo en C");
        shown = {digit(hex[5]), digit(hex[4]), " ", digit(hex[3]), digit(hex[2]), ":",
                 digit(hex[1]), digit(hex[0])};
        if (shown != "03 02:05") begin
            $error("displays '%s', se esperaba '03 02:05'", shown); errors++;
        end else
            $display("OK    displays muestran '%s'", shown);
        if (errors == 0) $display("TB_FILTRO: PASS");
        else             $display("TB_FILTRO: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
