// Respuesta en frecuencia MEDIDA del modulo propio (evidencia de PR-10 en simulacion).
// Pasa senos de 30 frecuencias (50 Hz - 20 kHz, escala logaritmica, amplitud 0.5) por
// filtro_audio en los dos bancos, solo por su interfaz Avalon-MM, y mide la amplitud
// RMS de la salida despues del transitorio. Escribe build/sim/filtro_respuesta.csv;
// scripts/plot_respuesta.py la compara con la respuesta teorica.
//
// Uso: scripts/sim-rtl.sh filtro_respuesta && python3 scripts/plot_respuesta.py
`timescale 1ns / 1ps

module tb_filtro_respuesta;
    logic clk = 1'b0, reset = 1'b1;
    always #10 clk = ~clk;
    logic [2:0]  address = '0;
    logic        read = 1'b0, write = 1'b0;
    logic [31:0] writedata = '0;
    wire  [31:0] readdata;
    wire         waitrequest;
    string vec_dir;

    filtro_audio dut (
        .clk (clk), .reset (reset),
        .avs_address (address), .avs_read (read), .avs_write (write),
        .avs_writedata (writedata), .avs_readdata (readdata), .avs_waitrequest (waitrequest)
    );

    localparam CTRL = 0, STATUS = 1, NTAPS = 2, GAIN = 3, CADDR = 4, CDATA = 5, DIN = 6, DOUT = 7;
    localparam real FS = 48000.0, PI = 3.14159265358979;
    localparam int  NF = 30, SETTLE = 160, MEAS = 480;

    task automatic avs_wr(input int a, input logic [31:0] d);
        @(posedge clk); address <= a; writedata <= d; write <= 1'b1;
        do @(posedge clk); while (waitrequest);
        write <= 1'b0;
    endtask
    task automatic avs_rd(input int a, output logic [31:0] d);
        @(posedge clk); address <= a; read <= 1'b1;
        do @(posedge clk); while (waitrequest);
        d = readdata; read <= 1'b0;
    endtask

    initial begin
        logic [15:0] h [0:255];
        logic [31:0] d;
        int fd;
        if (!$value$plusargs("VEC=%s", vec_dir)) vec_dir = "../../sim/vectors";
        $display("TB_FILTRO_RESPUESTA: inicio");
        repeat (5) @(posedge clk);
        reset <= 1'b0;
        do avs_rd(STATUS, d); while (d[1]);
        $readmemh({vec_dir, "/../../hw/rtl/fir_coefs.hex"}, h);
        avs_wr(CADDR, 0);
        for (int i = 0; i < 256; i++) begin
            if (i == 128) avs_wr(CADDR, 8'h80);
            avs_wr(CDATA, {16'd0, h[i]});
        end
        fd = $fopen({vec_dir, "/../../build/sim/filtro_respuesta.csv"}, "w");
        $fwrite(fd, "banco,frecuencia_hz,ganancia_db\n");
        for (int bank = 0; bank < 2; bank++) begin
            for (int k = 0; k < NF; k++) begin
                automatic real f = 50.0 * (400.0 ** (real'(k) / (NF - 1)));   // 50 Hz .. 20 kHz
                automatic real sum_in = 0.0, sum_out = 0.0, g_db;
                avs_wr(CTRL, {29'd0, 1'b1, bank[0], 1'b0});          // CLEAR + banco
                for (int n = 0; n < SETTLE + MEAS; n++) begin
                    automatic int x = $rtoi(16384.0 * $sin(2.0 * PI * f * n / FS));
                    logic signed [15:0] y;
                    avs_wr(DIN, {x[15:0], x[15:0]});
                    avs_rd(DOUT, d);
                    y = d[15:0];
                    if (n >= SETTLE) begin
                        sum_in  += real'(x) * real'(x);
                        sum_out += real'(y) * real'(y);
                    end
                end
                g_db = 10.0 * $log10((sum_out + 1e-9) / sum_in);
                $fwrite(fd, "%0d,%.1f,%.2f\n", bank, f, g_db);
            end
            $display("OK    banco %0d: %0d frecuencias medidas", bank, NF);
        end
        $fclose(fd);
        $display("TB_FILTRO_RESPUESTA: PASS");
        $finish;
    end
endmodule
