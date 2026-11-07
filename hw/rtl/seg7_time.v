// Displays de 7 segmentos del reproductor (HW-10): esclavo Avalon-MM que muestra el
// tiempo transcurrido MM:SS en HEX3..HEX0 y, opcionalmente, el numero de cancion en
// HEX5..HEX4. Verilog-2005 sintetizable (CD-13).
//
// El software solo escribe el tiempo en segundos; el hardware lo pasa a MM:SS y a BCD
// con un convertidor secuencial por restas (sin divisores, <= 110 ciclos tras cada
// escritura) y decodifica los segmentos. Asi el "manejo directo de los displays"
// (§3-HW p.4) queda en hardware, que es la lectura conservadora de P6.
//
//  Off  Nombre  Acceso  Contenido
//  0x0  CTRL    RW      [0] ON (0: todos apagados)  [1] TRACK (muestra la cancion en HEX5..4)
//  0x4  TIME    RW      [12:0] segundos; de 6000 en adelante se muestra 99:59
//  0x8  TRACK   RW      [6:0] numero de cancion; de 100 en adelante se muestra 99
//  0xC  STATUS  R       [15:0] BCD mostrado {M1, M0, S1, S0}  [16] BUSY (convirtiendo)
//                       [31:24] version
//
// Salidas activas en bajo (segmento encendido = 0), orden {g, f, e, d, c, b, a} del
// DE1-SoC (HEXn[0] = a ... HEXn[6] = g).
module seg7_time (
    input  wire        clk,
    input  wire        reset,

    input  wire [1:0]  avs_address,
    input  wire        avs_read,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    output reg  [31:0] avs_readdata,

    output wire [6:0]  hex0,
    output wire [6:0]  hex1,
    output wire [6:0]  hex2,
    output wire [6:0]  hex3,
    output wire [6:0]  hex4,
    output wire [6:0]  hex5
);
    localparam [7:0] VERSION = 8'h01;

    reg        on, track_on;
    reg [12:0] time_s;
    reg [6:0]  track;

    // ---------------------------------------------------------------- conversion
    // Estados: restar 60 (minutos), restar 10 (decenas de segundo), restar 10
    // (decenas de minuto), listo. Con time_s < 6000 son <= 99 + 5 + 9 iteraciones.
    localparam C_IDLE = 2'd0, C_MIN = 2'd1, C_SEC = 2'd2, C_MTEN = 2'd3;
    reg [1:0]  cst;
    reg [12:0] rem;
    reg [6:0]  mins;
    reg [3:0]  s1, m1;
    reg [15:0] bcd;                        // lo que se muestra {M1, M0, S1, S0}
    reg [3:0]  t1, t0;                     // cancion en BCD

    wire convert = avs_write && avs_address == 2'd1;

    always @(posedge clk) begin
        if (reset) begin
            cst <= C_IDLE;
            bcd <= 16'h0000;
        end else case (cst)
            C_IDLE: if (convert) begin
                rem  <= (avs_writedata > 32'd5999) ? 13'd5999 : avs_writedata[12:0];
                mins <= 7'd0;
                cst  <= C_MIN;
            end
            C_MIN: if (rem >= 13'd60) begin
                rem  <= rem - 13'd60;
                mins <= mins + 7'd1;
            end else begin
                s1  <= 4'd0;
                cst <= C_SEC;
            end
            C_SEC: if (rem >= 13'd10) begin       // rem < 60: segundos
                rem <= rem - 13'd10;
                s1  <= s1 + 4'd1;
            end else begin
                m1  <= 4'd0;
                cst <= C_MTEN;
            end
            C_MTEN: if (mins >= 7'd10) begin
                mins <= mins - 7'd10;
                m1   <= m1 + 4'd1;
            end else begin
                bcd <= {m1, mins[3:0], s1, rem[3:0]};
                cst <= C_IDLE;
            end
        endcase
    end

    // Cancion 0..99 a BCD (combinacional: 7 bits, barato)
    always @(*) begin
        t1 = (track >= 7'd90) ? 4'd9 : (track >= 7'd80) ? 4'd8 : (track >= 7'd70) ? 4'd7 :
             (track >= 7'd60) ? 4'd6 : (track >= 7'd50) ? 4'd5 : (track >= 7'd40) ? 4'd4 :
             (track >= 7'd30) ? 4'd3 : (track >= 7'd20) ? 4'd2 : (track >= 7'd10) ? 4'd1 : 4'd0;
        t0 = track - t1 * 7'd10;
    end

    // ---------------------------------------------------------------- registros
    always @(posedge clk) begin
        if (reset) begin
            on       <= 1'b1;              // arranca mostrando 00:00
            track_on <= 1'b0;
            time_s   <= 13'd0;
            track    <= 7'd0;
        end else if (avs_write) begin
            case (avs_address)
            2'd0: begin on <= avs_writedata[0]; track_on <= avs_writedata[1]; end
            2'd1: time_s <= avs_writedata[12:0];
            2'd2: track  <= (avs_writedata > 32'd99) ? 7'd99 : avs_writedata[6:0];  // compara antes de truncar
            default: ;
            endcase
        end
    end

    always @(posedge clk)
        if (avs_read)
            case (avs_address)
            2'd0: avs_readdata <= {30'd0, track_on, on};
            2'd1: avs_readdata <= {19'd0, time_s};
            2'd2: avs_readdata <= {25'd0, track};
            2'd3: avs_readdata <= {VERSION, 7'd0, cst != C_IDLE, bcd};
            endcase

    // ---------------------------------------------------------------- segmentos
    function [6:0] seg;                    // activo en bajo, {g,f,e,d,c,b,a}
        input [3:0] d;
        case (d)
        4'd0: seg = 7'b1000000;  4'd1: seg = 7'b1111001;  4'd2: seg = 7'b0100100;
        4'd3: seg = 7'b0110000;  4'd4: seg = 7'b0011001;  4'd5: seg = 7'b0010010;
        4'd6: seg = 7'b0000010;  4'd7: seg = 7'b1111000;  4'd8: seg = 7'b0000000;
        4'd9: seg = 7'b0010000;  default: seg = 7'b1111111;
        endcase
    endfunction

    localparam [6:0] OFF = 7'b1111111;
    assign hex0 = on ? seg(bcd[3:0])   : OFF;
    assign hex1 = on ? seg(bcd[7:4])   : OFF;
    assign hex2 = on ? seg(bcd[11:8])  : OFF;
    assign hex3 = on ? seg(bcd[15:12]) : OFF;
    assign hex4 = (on && track_on) ? seg(t0) : OFF;
    assign hex5 = (on && track_on) ? seg(t1) : OFF;
endmodule
