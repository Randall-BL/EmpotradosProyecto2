// Verificacion aislada de seg7_time (HW-10, DK-13 en Questa). Escribe tiempos por
// Avalon-MM, espera la conversion y decodifica los segmentos de vuelta a digitos:
// 00:00, 00:59, 02:05, 09:59, 59:59, 99:59, saturacion de 6000 s a 99:59, cancion en
// HEX5..4 (07, saturacion a 99), y apagado (CTRL.ON = 0).
//
// Uso: scripts/sim-rtl.sh seg7_time
`timescale 1ns / 1ps

module tb_seg7_time;
    logic clk = 1'b0, reset = 1'b1;
    always #10 clk = ~clk;

    logic [1:0]  address = '0;
    logic        read = 1'b0, write = 1'b0;
    logic [31:0] writedata = '0;
    wire  [31:0] readdata;
    wire  [6:0]  hex [6];

    seg7_time dut (
        .clk (clk), .reset (reset),
        .avs_address (address), .avs_read (read), .avs_write (write),
        .avs_writedata (writedata), .avs_readdata (readdata),
        .hex0 (hex[0]), .hex1 (hex[1]), .hex2 (hex[2]), .hex3 (hex[3]), .hex4 (hex[4]), .hex5 (hex[5])
    );

    int errors = 0, checks = 0;

    task automatic wr(input int a, input logic [31:0] d);
        @(posedge clk); address <= a; writedata <= d; write <= 1'b1;
        @(posedge clk); write <= 1'b0;
    endtask

    task automatic rd(input int a, output logic [31:0] d);   // latencia de lectura 1
        @(posedge clk); address <= a; read <= 1'b1;
        @(posedge clk); read <= 1'b0;
        @(negedge clk); d = readdata;
    endtask

    // Segmentos (activos en bajo) -> caracter: '0'..'9', ' ' apagado, '?' invalido
    function automatic string digit(input logic [6:0] s);
        case (s)
        7'b1000000: return "0";  7'b1111001: return "1";  7'b0100100: return "2";
        7'b0110000: return "3";  7'b0011001: return "4";  7'b0010010: return "5";
        7'b0000010: return "6";  7'b1111000: return "7";  7'b0000000: return "8";
        7'b0010000: return "9";  7'b1111111: return " ";  default:    return "?";
        endcase
    endfunction

    function automatic string shown();
        return {digit(hex[5]), digit(hex[4]), " ", digit(hex[3]), digit(hex[2]), ":",
                digit(hex[1]), digit(hex[0])};
    endfunction

    task automatic show_time(input int secs, input string exp);
        logic [31:0] st;
        wr(1, secs);
        do rd(3, st); while (st[16]);                        // espera la conversion
        checks++;
        if (shown() != exp) begin
            errors++;
            $error("TIME = %0d s: displays '%s', se esperaba '%s'", secs, shown(), exp);
        end else
            $display("OK    TIME = %4d s -> '%s'", secs, shown());
    endtask

    task automatic expect_shown(input string what, input string exp);
        checks++;
        if (shown() != exp) begin errors++; $error("%s: '%s', se esperaba '%s'", what, shown(), exp); end
        else $display("OK    %s -> '%s'", what, shown());
    endtask

    initial begin
        logic [31:0] d;
        $display("TB_SEG7_TIME: inicio");
        repeat (3) @(posedge clk);
        reset <= 1'b0;
        @(negedge clk);
        expect_shown("reset", "   00:00");
        show_time(0,    "   00:00");
        show_time(59,   "   00:59");
        show_time(125,  "   02:05");
        show_time(599,  "   09:59");
        show_time(3599, "   59:59");
        show_time(5999, "   99:59");
        show_time(6000, "   99:59");
        show_time(8191, "   99:59");
        wr(2, 7);   wr(0, 3); @(negedge clk); expect_shown("cancion 7 en HEX5..4", "07 99:59");
        wr(2, 150);           @(negedge clk); expect_shown("cancion 150 se satura a 99", "99 99:59");
        wr(0, 0);             @(negedge clk); expect_shown("CTRL.ON = 0: apagado (el : lo pone el tb)", "     :  ");
        wr(0, 1);             @(negedge clk); expect_shown("encendido sin cancion", "   99:59");
        rd(3, d); checks++;
        if (d[31:24] !== 8'h01 || d[15:0] !== 16'h9959) begin
            errors++; $error("STATUS = 0x%08h", d);
        end
        $display("Comprobaciones: %0d", checks);
        if (errors == 0) $display("TB_SEG7_TIME: PASS");
        else             $display("TB_SEG7_TIME: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
