// Modelo de comportamiento de los ADC de la DE1-SoC, segun sus datasheets:
// las revisiones viejas traen AD7928 y las nuevas LTC2308, en los mismos pines
// (ADC_CONVST es CS_N en el AD7928). LTC = 0 modela el AD7928 y LTC = 1 el
// LTC2308. ch_value[n] es el voltaje "analogico" del canal n en cuentas de 12 bits.
//
// AD7928: SCLK en reposo alto. Al bajar CS_N saca un cero; luego, en cada flanco
//   de bajada de SCLK: ADD2..ADD0 del canal convertido y DB11..DB0. Toma DIN en
//   los flancos de bajada (12 bits de control, MSB primero): WRITE, SEQ, -, ADD2..0,
//   PM1..0, SHADOW, -, RANGE, CODING. Con WRITE=1, segun SEQ/SHADOW (tabla del
//   datasheet): 0/0 canal fijo ADD; 1/1 secuencia de los canales 0..ADD; 1/0 sigue
//   la secuencia ya programada; 0/1 programa el registro de sombra (aqui: canal
//   ADD). Con WRITE=0 el registro no cambia y la secuencia sigue avanzando.
// LTC2308: SCLK en reposo bajo. La conversion arranca al subir CONVST; al bajar,
//   SDO saca B11 y cambia en cada flanco de bajada de SCLK. Toma SDI en los flancos
//   de subida (6 bits: S/D, O/S, S1, S0, UNI, SLP); esa configuracion vale para la
//   conversion SIGUIENTE (canal = {S1, S0, O/S} en modo single-ended).
`timescale 1ns / 1ps

module adc_model #(
    parameter bit LTC = 0
) (
    input  wire        cs_n,     // ADC_CONVST
    input  wire        sclk,
    input  wire        din,
    output logic       dout
);
    logic [11:0] ch_value [0:7];
    initial foreach (ch_value[i]) ch_value[i] = 12'h100 * (i + 1);

    int conversions = 0;

    // ---------------- AD7928 ----------------
    generate if (!LTC) begin : g_ad7928
        logic [15:0] out_sr;
        logic [11:0] ctrl_sr;
        int          nbits;
        logic [2:0]  cur_ch = 3'd0, seq_max = 3'd0;
        bit          seq = 0;

        always @(negedge cs_n) begin
            out_sr = {1'b0, cur_ch, ch_value[cur_ch]};
            nbits  = 0;
            #5 dout = out_sr[15];
            conversions++;
        end
        always @(posedge cs_n) dout = 1'bz;

        always @(negedge sclk) if (!cs_n) begin
            if (nbits < 12) ctrl_sr = {ctrl_sr[10:0], din};
            nbits++;
            out_sr = {out_sr[14:0], 1'b0};
            #5 dout = out_sr[15];
            if (nbits == 16) begin
                // Fin de la trama: decide el canal de la siguiente conversion.
                automatic bit wr = ctrl_sr[11], sq = ctrl_sr[10], sh = ctrl_sr[3];
                if (wr && sq && sh) begin                   // nueva secuencia 0..ADD
                    seq = 1; seq_max = ctrl_sr[8:6]; cur_ch = 3'd0;
                end else if (wr && !sq) begin               // canal fijo
                    seq = 0; cur_ch = ctrl_sr[8:6];
                end else if (seq) begin                     // sigue la secuencia
                    cur_ch = (cur_ch == seq_max) ? 3'd0 : cur_ch + 3'd1;
                end
            end
        end
    end else begin : g_ltc2308
        logic [11:0] out_sr;
        logic [5:0]  cfg_sr;
        int          nbits;
        logic [2:0]  next_ch = 3'd0, conv_ch = 3'd0;
        logic [11:0] result = 12'd0;

        // CONVST sube: convierte el canal que dejo la trama anterior.
        always @(posedge cs_n) begin
            conv_ch = next_ch;
            result  = ch_value[conv_ch];
            conversions++;
        end
        always @(negedge cs_n) begin
            out_sr = result;
            nbits  = 0;
            #5 dout = out_sr[11];
        end
        always @(posedge sclk) if (!cs_n) begin
            if (nbits < 6) begin
                cfg_sr = {cfg_sr[4:0], din};
                if (nbits == 5 && cfg_sr[5])                 // S/D = 1: single-ended
                    next_ch = {cfg_sr[3], cfg_sr[2], cfg_sr[4]};  // {S1, S0, O/S}
            end
            nbits++;
        end
        always @(negedge sclk) if (!cs_n) begin
            out_sr = {out_sr[10:0], 1'b0};
            #5 dout = out_sr[11];
        end
    end endgenerate
endmodule
