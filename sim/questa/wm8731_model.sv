// Modelo de comportamiento del codec WM8731 de la DE1-SoC para los testbenches.
//
// - Interfaz digital de audio en modo maestro, Left Justified, 16 bits (lo que
//   configura el SoC, CD-07): genera BCLK = MCLK/4 = 3.072 MHz y DACLRCK = 48 kHz
//   (32 BCLK por canal); DACLRCK cambia en el flanco de bajada de BCLK; en alto
//   es el canal izquierdo. Muestrea DACDAT en los flancos de subida de BCLK: el
//   MSB llega en el primer flanco de subida despues del cambio de DACLRCK.
//   Cada par recibido se publica en (left, right) con un pulso en frame_valid.
// - Esclavo I2C en la direccion 0x34 (escritura): reconoce cada byte y publica
//   cada escritura de registro (7 bits) y dato (9 bits) con un pulso en reg_valid.
//   Guarda el ultimo valor de cada registro en regs[]. Tambien reconoce (ACK) las
//   transferencias a otras direcciones del mismo bus, como el decodificador de
//   video ADV7180 (0x40) que la autoinicializacion de la DE1-SoC configura
//   despues del codec, y las cuenta en other_writes.
//
// Simplificaciones: no modela el ADC del codec ni lecturas I2C (el WM8731 no las
// tiene), ni tiempos de establecimiento de I2C.
`timescale 1ns / 1ps

module wm8731_model #(
    parameter real BCLK_NS = 1.0e9 / 3.072e6
) (
    input  wire        enable,       // 1 = genera BCLK/LRCK
    // Audio
    output logic       bclk,
    output logic       daclrck,
    input  wire        dacdat,
    output logic [15:0] left,
    output logic [15:0] right,
    output logic       frame_valid,
    // I2C
    input  wire        scl,
    inout  wire        sda
);
    // ---------------- Audio ----------------
    initial begin
        bclk = 1'b0;
        daclrck = 1'b0;
        frame_valid = 1'b0;
        left = '0;
        right = '0;
    end

    always begin
        #(BCLK_NS / 2.0);
        if (enable) bclk = ~bclk;
    end

    int bit_cnt = 0;              // BCLK de bajada dentro del canal (0..31)
    always @(negedge bclk) begin
        bit_cnt = (bit_cnt + 1) % 32;
        if (bit_cnt == 0) daclrck = ~daclrck;
    end

    logic [15:0] shift;
    int rx_bit = 0;
    always @(posedge daclrck or negedge daclrck) rx_bit = 0;
    always @(posedge bclk) begin
        if (rx_bit < 16) begin
            shift = {shift[14:0], dacdat};
            rx_bit++;
            if (rx_bit == 16) begin
                if (daclrck) left = shift;
                else begin
                    right = shift;
                    frame_valid = 1'b1;      // par completo al terminar el derecho
                    #1 frame_valid = 1'b0;
                end
            end
        end
    end

    // ---------------- I2C ----------------
    logic       sda_drive_low = 1'b0;
    assign sda = sda_drive_low ? 1'b0 : 1'bz;

    logic [8:0] regs [0:15];
    logic [6:0] reg_addr;
    logic [8:0] reg_data;
    logic       reg_valid = 1'b0;
    int         writes = 0;
    int         other_writes = 0;
    initial foreach (regs[i]) regs[i] = 'x;

    // Una transferencia: START, 3 bytes (direccion, B15..B8, B7..B0) con ACK, STOP.
    logic [7:0] byte_sr;
    int         nbits, nbytes;
    logic [7:0] b [0:2];
    bit         in_frame = 0;

    bit debug = 0;                                     // tb: codec.debug = 1
    always @(negedge sda) if (scl === 1'b1) begin      // START
        if (debug) $display("[%0t] I2C START (in_frame=%0d nbytes=%0d)", $time, in_frame, nbytes);
        in_frame = 1; nbits = 0; nbytes = 0;
    end
    always @(posedge sda) if (scl === 1'b1 && in_frame) begin   // STOP
        if (debug) $display("[%0t] I2C STOP nbytes=%0d b=%h %h %h", $time, nbytes, b[0], b[1], b[2]);
        in_frame = 0;
        if (nbytes == 3 && b[0] == 8'h34) begin
            reg_addr = b[1][7:1];
            reg_data = {b[1][0], b[2]};
            regs[reg_addr] = reg_data;
            writes++;
            reg_valid = 1'b1;
            #1 reg_valid = 1'b0;
        end else if (nbytes == 3)
            other_writes++;
    end
    always @(posedge scl) if (in_frame && nbits < 8) begin
        byte_sr = {byte_sr[6:0], (sda === 1'b0) ? 1'b0 : 1'b1};
        nbits++;
    end
    always @(negedge scl) if (in_frame) begin
        if (nbits == 8) begin                 // 8 bits recibidos: ACK en el 9.o
            if (nbytes < 3) b[nbytes] = byte_sr;
            nbytes++;
            sda_drive_low = 1'b1;
            nbits = 9;
        end else if (nbits == 9) begin        // fin del ACK
            sda_drive_low = 1'b0;
            nbits = 0;
        end
    end
endmodule
