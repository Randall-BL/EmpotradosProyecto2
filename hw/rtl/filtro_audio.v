// Modulo de hardware personalizado del reproductor (HW-14): esclavo Avalon-MM con
// registros de control, estado y datos alrededor de fir_core (filtro FIR estereo de
// 2 bancos + ganancia). Mapa de registros: docs/hw/modulo-filtro.md.
//
//  Off  Nombre     Acceso  Contenido
//  0x00 CTRL       RW      [0] BYPASS  [1] BANK  [2] CLEAR (escribir 1: borra la linea
//                          de retardo; se lee 0)
//  0x04 STATUS     R/W1C   [0] READY (hay resultado y esta libre)  [1] BUSY
//                          [2] SAT (pegajoso, escribir 1 lo borra)  [15:8] MAX_TAPS
//                          [31:16] VERSION
//  0x08 NTAPS      RW      [7:0] taps 1..128 (0 se toma como 1, > 128 como 128)
//  0x0C GAIN       RW      [15:0] ganancia Q1.15 sin signo (0x8000 = 1.0, max ~2.0)
//  0x10 COEF_ADDR  RW      [6:0] indice  [7] banco
//  0x14 COEF_DATA  RW      escribir: coef[COEF_ADDR] = dato[15:0] y COEF_ADDR++ (dentro
//                          del banco); leer: coef[COEF_ADDR]
//  0x18 DATA_IN    RW      escribir {R[31:16], L[15:0]} procesa un par de muestras;
//                          leer devuelve la ultima entrada
//  0x1C DATA_OUT   R       {R, L} del ultimo par procesado
//
// waitrequest: mientras el filtro trabaja (BUSY), se retienen las escrituras a DATA_IN,
// COEF_DATA y CTRL.CLEAR y las lecturas de DATA_OUT y COEF_DATA; asi el software escribe
// una muestra y lee el resultado sin sondear. Toda lectura tiene un ciclo de espera
// (dato registrado). Solo accesos de 32 bits.
module filtro_audio #(
    parameter COEF_FILE = ""
) (
    input  wire        clk,
    input  wire        reset,

    input  wire [2:0]  avs_address,
    input  wire        avs_read,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    output reg  [31:0] avs_readdata,
    output wire        avs_waitrequest
);
    localparam [15:0] VERSION  = 16'h0100;
    localparam        MAX_TAPS = 128;

    localparam A_CTRL = 3'd0, A_STATUS = 3'd1, A_NTAPS = 3'd2, A_GAIN = 3'd3,
               A_CADDR = 3'd4, A_CDATA = 3'd5, A_DIN = 3'd6, A_DOUT = 3'd7;

    reg        bypass, bank;
    reg  [7:0] ntaps;
    reg [15:0] gain;
    reg  [7:0] coef_addr;                  // {banco, indice}
    reg [31:0] din;
    reg        sat_sticky, ready;

    wire        busy, done, core_sat;
    wire [15:0] y_l, y_r, coef_rdata;

    // ---------------------------------------------------------------- espera
    wire blocking_wr = avs_write && (avs_address == A_DIN || avs_address == A_CDATA ||
                                     (avs_address == A_CTRL && avs_writedata[2]));
    wire blocking_rd = avs_read && (avs_address == A_DOUT || avs_address == A_CDATA);
    reg  rd_wait;                          // primer ciclo de cada lectura
    // start/clear se emiten un solo ciclo: la escritura que espera por BUSY se acepta
    // en el primer ciclo con busy = 0, y ese mismo ciclo arranca el nucleo.
    wire wr_ok = avs_write && !(blocking_wr && busy);
    assign avs_waitrequest = (avs_write && blocking_wr && busy) ||
                             (avs_read && (rd_wait || (blocking_rd && busy)));

    always @(posedge clk) begin
        if (reset)
            rd_wait <= 1'b1;
        else if (avs_read && !(blocking_rd && busy))
            rd_wait <= ~rd_wait;           // 1 -> 0: dato listo el siguiente ciclo
        else
            rd_wait <= 1'b1;
    end

    // ---------------------------------------------------------------- nucleo
    wire start = wr_ok && avs_address == A_DIN;
    wire clear = wr_ok && avs_address == A_CTRL && avs_writedata[2];
    wire coef_we = wr_ok && avs_address == A_CDATA;

    fir_core #(.MAX_TAPS(MAX_TAPS), .AW(7), .COEF_FILE(COEF_FILE)) u_core (
        .clk        (clk),
        .reset      (reset),
        .start      (start),
        .x_l        (avs_writedata[15:0]),
        .x_r        (avs_writedata[31:16]),
        .bank       (bank),
        .ntaps      (ntaps),
        .bypass     (bypass),
        .gain       (gain),
        .clear      (clear),
        .busy       (busy),
        .done       (done),
        .y_l        (y_l),
        .y_r        (y_r),
        .sat        (core_sat),
        .coef_we    (coef_we),
        .coef_addr  (coef_addr),
        .coef_wdata (avs_writedata[15:0]),
        .coef_rdata (coef_rdata)
    );

    // ---------------------------------------------------------------- registros
    always @(posedge clk) begin
        if (reset) begin
            bypass     <= 1'b1;            // arranca en bypass: el audio pasa sin filtrar
            bank       <= 1'b0;
            ntaps      <= 8'd127;
            gain       <= 16'h8000;        // 1.0
            coef_addr  <= 8'd0;
            din        <= 32'd0;
            sat_sticky <= 1'b0;
            ready      <= 1'b0;
        end else begin
            if (done) begin
                ready <= 1'b1;
                if (core_sat) sat_sticky <= 1'b1;
            end
            if (start) begin
                din   <= avs_writedata;
                ready <= 1'b0;
            end
            if (wr_ok) begin
                case (avs_address)
                A_CTRL:   begin bypass <= avs_writedata[0]; bank <= avs_writedata[1]; end
                A_STATUS: if (avs_writedata[2]) sat_sticky <= 1'b0;
                A_NTAPS:  ntaps <= (avs_writedata[7:0] == 8'd0) ? 8'd1 :
                                   (avs_writedata[7:0] > MAX_TAPS) ? MAX_TAPS[7:0] :
                                   avs_writedata[7:0];
                A_GAIN:   gain <= avs_writedata[15:0];
                A_CADDR:  coef_addr <= avs_writedata[7:0];
                A_CDATA:  coef_addr <= {coef_addr[7], coef_addr[6:0] + 7'd1};
                default:  ;
                endcase
            end
        end
    end

    // ---------------------------------------------------------------- lectura
    always @(posedge clk) begin
        if (avs_read) begin
            case (avs_address)
            A_CTRL:   avs_readdata <= {29'd0, 1'b0, bank, bypass};
            A_STATUS: avs_readdata <= {VERSION, MAX_TAPS[7:0], 5'd0, sat_sticky, busy, ready && !busy};
            A_NTAPS:  avs_readdata <= {24'd0, ntaps};
            A_GAIN:   avs_readdata <= {16'd0, gain};
            A_CADDR:  avs_readdata <= {24'd0, coef_addr};
            A_CDATA:  avs_readdata <= {{16{coef_rdata[15]}}, coef_rdata};
            A_DIN:    avs_readdata <= din;
            A_DOUT:   avs_readdata <= {y_r, y_l};
            default:  avs_readdata <= 32'd0;
            endcase
        end
    end
endmodule
