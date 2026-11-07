// Nucleo del filtro FIR estereo del modulo propio (HW-13). Verilog-2005 sintetizable,
// sin primitivas de Altera (CD-13): memorias y multiplicadores inferidos.
//
// Por cada par de muestras (start):
//   1. LOAD   escribe x en la linea de retardo circular de cada canal
//   2. MAC    recorre los ntaps coeficientes del banco con dos multiplicadores-
//             acumuladores en paralelo (uno por canal), un tap por ciclo, con un
//             pipeline de 3 etapas: lectura de RAM -> producto -> acumulado
//   3. ROUND  y = sat16((acc + 2^14) >>> 15)      (o y = x en bypass, sin MAC)
//   4. GAIN   z = sat16((y * g + 2^14) >>> 15)    g Q1.15 sin signo (volumen, CD-05)
// Formato de punto fijo: docs/decisiones/CD-14.md. El modelo de referencia bit a bit
// es FirModel en scripts/design_fir.py.
//
// Latencia: ntaps + 6 ciclos (127 taps: 133 ciclos = 2.7 us a 50 MHz; a 48 kHz hay
// 1041 ciclos por muestra). La configuracion (banco, taps, bypass, ganancia) se toma
// al recibir start: cambiarla en medio de una muestra no la corrompe.
// Tras el reset recorre la linea de retardo poniendola en cero (128 ciclos).
module fir_core #(
    parameter MAX_TAPS  = 128,             // potencia de 2
    parameter AW        = 7,               // log2(MAX_TAPS)
    parameter COEF_FILE = ""               // $readmemh opcional: 2*MAX_TAPS coeficientes
) (
    input  wire              clk,
    input  wire              reset,

    // Una muestra estereo
    input  wire              start,
    input  wire signed [15:0] x_l,
    input  wire signed [15:0] x_r,
    input  wire              bank,
    input  wire [AW:0]       ntaps,         // 1..MAX_TAPS
    input  wire              bypass,
    input  wire [15:0]       gain,          // Q1.15 sin signo
    input  wire              clear,         // pone la linea de retardo en cero

    output reg               busy,
    output reg               done,          // pulso: y_l/y_r validos
    output reg signed [15:0] y_l,
    output reg signed [15:0] y_r,
    output reg               sat,           // pulso con done: hubo saturacion

    // Banco de coeficientes (lo escribe y lee el procesador cuando busy = 0)
    input  wire              coef_we,
    input  wire [AW:0]       coef_addr,     // {banco, indice}
    input  wire [15:0]       coef_wdata,
    output reg  [15:0]       coef_rdata
);
    localparam S_CLEAR = 3'd0, S_IDLE = 3'd1, S_LOAD = 3'd2, S_MAC = 3'd3,
               S_DRAIN = 3'd4, S_ROUND = 3'd5, S_GAIN = 3'd6, S_DONE = 3'd7;

    // ---------------------------------------------------------------- memorias
    reg signed [15:0] coef_mem [0:2*MAX_TAPS-1];
    reg signed [15:0] dl_l [0:MAX_TAPS-1];
    reg signed [15:0] dl_r [0:MAX_TAPS-1];

    integer i;
    initial begin
        if (COEF_FILE != "")
            $readmemh(COEF_FILE, coef_mem);
        else
            for (i = 0; i < 2*MAX_TAPS; i = i + 1) coef_mem[i] = 16'sd0;
    end

    reg  [2:0]       state;
    reg  [AW-1:0]    pos;                  // muestra mas reciente
    reg  [AW:0]      k;                    // tap que se emite
    reg  [AW:0]      n_lat;                // taps latcheados
    reg              bank_lat, bypass_lat;
    reg  [15:0]      gain_lat;
    reg  signed [15:0] xl_lat, xr_lat;

    // Escritura de la linea de retardo (LOAD) y borrado (CLEAR)
    wire             dl_we    = (state == S_LOAD) || (state == S_CLEAR);
    wire [AW-1:0]    dl_waddr = (state == S_CLEAR) ? k[AW-1:0] : pos - 1'b1;
    wire signed [15:0] dl_wl  = (state == S_CLEAR) ? 16'sd0 : xl_lat;
    wire signed [15:0] dl_wr  = (state == S_CLEAR) ? 16'sd0 : xr_lat;
    always @(posedge clk)
        if (dl_we) begin
            dl_l[dl_waddr] <= dl_wl;
            dl_r[dl_waddr] <= dl_wr;
        end

    // Lectura sincrona (un ciclo) de coeficiente y muestras para el MAC
    wire [AW-1:0]    dl_raddr = pos + k[AW-1:0];
    reg  signed [15:0] c_q, xl_q, xr_q;
    always @(posedge clk) begin
        c_q  <= coef_mem[{bank_lat, k[AW-1:0]}];
        xl_q <= dl_l[dl_raddr];
        xr_q <= dl_r[dl_raddr];
    end

    // Puerto del procesador al banco de coeficientes
    always @(posedge clk) begin
        if (coef_we && !busy)
            coef_mem[coef_addr] <= coef_wdata;
        coef_rdata <= coef_mem[coef_addr];
    end

    // ---------------------------------------------------------------- MAC
    reg               v1, v2;              // valido en la etapa de producto / acumulado
    reg signed [31:0] p_l, p_r;
    reg signed [39:0] acc_l, acc_r;
    wire issuing = (state == S_MAC);

    always @(posedge clk) begin
        v1  <= issuing;
        v2  <= v1;
        p_l <= c_q * xl_q;
        p_r <= c_q * xr_q;
        if (state == S_LOAD) begin
            acc_l <= 40'sd0;
            acc_r <= 40'sd0;
        end else if (v2) begin
            acc_l <= acc_l + $signed({{8{p_l[31]}}, p_l});
            acc_r <= acc_r + $signed({{8{p_r[31]}}, p_r});
        end
    end

    // ---------------------------------------------------------------- redondeo y saturacion
    function signed [15:0] sat16;
        input signed [40:0] v;
        begin
            if (v > 41'sd32767)       sat16 = 16'sd32767;
            else if (v < -41'sd32768) sat16 = -16'sd32768;
            else                      sat16 = v[15:0];
        end
    endfunction

    function is_sat;
        input signed [40:0] v;
        begin
            is_sat = (v > 41'sd32767) || (v < -41'sd32768);
        end
    endfunction

    // y = sat16((acc + 2^14) >>> 15)
    // $signed: una concatenacion es sin signo y >>> seria un desplazamiento logico.
    wire signed [40:0] rnd_l = ($signed({acc_l[39], acc_l}) + 41'sd16384) >>> 15;
    wire signed [40:0] rnd_r = ($signed({acc_r[39], acc_r}) + 41'sd16384) >>> 15;
    reg  signed [15:0] fy_l, fy_r;
    reg                sat_acc, sat_hold;

    // z = sat16((y * g + 2^14) >>> 15), g sin signo
    wire signed [33:0] gp_l = fy_l * $signed({1'b0, gain_lat});
    wire signed [33:0] gp_r = fy_r * $signed({1'b0, gain_lat});
    wire signed [40:0] gz_l = ($signed({{7{gp_l[33]}}, gp_l}) + 41'sd16384) >>> 15;
    wire signed [40:0] gz_r = ($signed({{7{gp_r[33]}}, gp_r}) + 41'sd16384) >>> 15;

    // ---------------------------------------------------------------- control
    always @(posedge clk) begin
        if (reset) begin
            state <= S_CLEAR;
            k     <= 0;
            pos   <= 0;
            busy  <= 1'b1;
            done  <= 1'b0;
            sat   <= 1'b0;
            y_l   <= 16'sd0;
            y_r   <= 16'sd0;
        end else begin
            done <= 1'b0;
            sat  <= 1'b0;
            case (state)
            S_CLEAR: begin                       // 128 ciclos escribiendo ceros
                if (k == MAX_TAPS - 1) begin
                    state <= S_IDLE;
                    busy  <= 1'b0;
                    k     <= 0;
                end else
                    k <= k + 1'b1;
            end
            S_IDLE: begin
                if (clear) begin
                    state <= S_CLEAR;
                    busy  <= 1'b1;
                    k     <= 0;
                end else if (start) begin
                    state      <= S_LOAD;
                    busy       <= 1'b1;
                    xl_lat     <= x_l;
                    xr_lat     <= x_r;
                    bank_lat   <= bank;
                    n_lat      <= (ntaps == 0) ? 1 : ((ntaps > MAX_TAPS) ? MAX_TAPS : ntaps);
                    bypass_lat <= bypass;
                    gain_lat   <= gain;
                end
            end
            S_LOAD: begin                        // la muestra entra en pos - 1
                pos     <= pos - 1'b1;
                k       <= 0;
                sat_acc <= 1'b0;
                if (bypass_lat) begin
                    fy_l  <= xl_lat;
                    fy_r  <= xr_lat;
                    state <= S_GAIN;
                end else
                    state <= S_MAC;
            end
            S_MAC: begin                         // emite un tap por ciclo
                if (k == n_lat - 1'b1)
                    state <= S_DRAIN;
                else
                    k <= k + 1'b1;
            end
            S_DRAIN: begin                       // espera que el pipeline vacie
                if (!issuing && !v1 && !v2)
                    state <= S_ROUND;
            end
            S_ROUND: begin
                fy_l    <= sat16(rnd_l);
                fy_r    <= sat16(rnd_r);
                sat_acc <= is_sat(rnd_l) || is_sat(rnd_r);
                state   <= S_GAIN;
            end
            S_GAIN: begin
                y_l      <= sat16(gz_l);
                y_r      <= sat16(gz_r);
                sat_hold <= sat_acc || is_sat(gz_l) || is_sat(gz_r);
                state    <= S_DONE;
            end
            S_DONE: begin                        // done y sat salen en el mismo ciclo
                done  <= 1'b1;
                sat   <= sat_hold;
                busy  <= 1'b0;
                state <= S_IDLE;
            end
            endcase
        end
    end
endmodule
