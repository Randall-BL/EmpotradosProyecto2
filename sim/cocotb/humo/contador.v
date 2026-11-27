// Contador de 8 bits con habilitacion: solo sirve para comprobar que la cadena
// cocotb + Icarus del contenedor funciona (DK-09). No forma parte del SoC.
module contador (
    input  wire       clk,
    input  wire       reset,     // sincrono, activo en alto
    input  wire       en,        // cuenta solo con en = 1
    output reg  [7:0] q
);
    always @(posedge clk)
        if (reset)   q <= 8'd0;
        else if (en) q <= q + 8'd1;
endmodule
