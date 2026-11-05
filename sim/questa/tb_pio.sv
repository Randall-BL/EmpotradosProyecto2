// Testbench autoverificable de los PIO del SoC (HW-02, PR-05).
//
// Simula el SoC completo (Nios V/m ejecutando sw/niosv/tests/test_pio) y hace de
// "tarjeta": maneja KEY[3:0] y SW[9:0] y comprueba LEDR[9:0]. El protocolo de
// etapas esta descrito en sw/niosv/tests/test_pio/main.c.
//
// Verifica: escritura y lectura de los PIO, IRQ por flanco de bajada de cada KEY,
// pulsacion simultanea, rebote mecanico (varios flancos -> un solo evento), IRQ
// de los switches y el tiempo de respuesta boton -> LED (§3-F p.4: < 200 ms).
//
// Uso: scripts/sim-tb.sh pio   (termina con "TB_PIO: PASS" o "TB_PIO: FAIL")
`timescale 1ns / 1ps

module tb_pio;
    localparam real CLK_NS      = 20.0;      // CLOCK_50
    localparam int  BOOT_US     = 3000;      // arranque del Nios V (crt0 + alt_main)
    localparam int  STEP_US     = 500;       // tiempo maximo por etapa
    localparam real RESP_MAX_US = 200000.0;  // requisito de respuesta a botones

    logic       clk   = 1'b0;
    logic       rst_n = 1'b0;
    logic [3:0] key   = 4'hF;                // KEY activos en bajo: sueltos = 1
    logic [9:0] sw    = 10'h000;
    wire  [9:0] led;

    int  errors   = 0;
    real resp_max = 0.0;

    always #(CLK_NS / 2.0) clk = ~clk;

    audio_soc dut (
        .clk_clk       (clk),
        .reset_reset_n (rst_n),
        .key_export    (key),
        .sw_export     (sw),
        .led_export    (led)
    );

    // Espera a que LEDR valga exp, con limite de tiempo. Devuelve el tiempo que tardo.
    task automatic expect_led(input logic [9:0] exp, input string what,
                              input int timeout_us, output real waited_ns);
        realtime t0 = $realtime;
        fork
            wait (led === exp);
            #(timeout_us * 1us);
        join_any
        disable fork;
        waited_ns = $realtime - t0;
        if (led !== exp) begin
            $error("[%0t] FALLO %s: LEDR = 0x%03h, se esperaba 0x%03h", $time, what, led, exp);
            errors++;
        end else begin
            $display("[%0t] OK    %s: LEDR = 0x%03h (%.2f us)", $time, what, led, waited_ns / 1000.0);
        end
    endtask

    // Pulsa los botones de mask; con bounce agrega 3 rebotes antes de asentarse.
    // Devuelve la respuesta medida desde el primer flanco de bajada hasta el LED.
    task automatic press(input logic [3:0] mask, input bit bounce, input logic [9:0] exp,
                         input string what);
        real waited;
        fork
            begin
                if (bounce) begin
                    repeat (3) begin
                        key = key & ~mask;  #300ns;
                        key = key |  mask;  #200ns;
                    end
                end
                key = key & ~mask;
                #20us;                       // pulsacion corta (en la tarjeta: ~100 ms)
                key = key | mask;
            end
            expect_led(exp, what, STEP_US, waited);
        join
        if (waited > resp_max) resp_max = waited;
        #20us;
    endtask

    initial begin
        real w;
        $display("TB_PIO: inicio");
        #(10 * CLK_NS) rst_n = 1'b1;

        expect_led(10'h2AA, "etapa 1, escritura de LEDR", BOOT_US, w);

        sw = 10'h155;
        expect_led(10'h155, "etapa 2, lectura de SW = 0x155", STEP_US, w);

        sw = 10'h000;
        expect_led(10'h300, "etapa 3, IRQ registradas", STEP_US, w);

        press(4'b0001, 1'b0, 10'h101, "KEY0 por IRQ");
        press(4'b0010, 1'b0, 10'h102, "KEY1 por IRQ");
        press(4'b0100, 1'b0, 10'h104, "KEY2 por IRQ");
        press(4'b1000, 1'b0, 10'h108, "KEY3 por IRQ");
        press(4'b0101, 1'b0, 10'h105, "KEY0 + KEY2 simultaneos");
        press(4'b0010, 1'b1, 10'h102, "KEY1 con rebote");

        sw = 10'h010;
        expect_led(10'h210, "IRQ de SW4 al subir", STEP_US, w);
        sw = 10'h018;
        expect_led(10'h218, "IRQ de SW3 al subir", STEP_US, w);

        $display("Respuesta maxima boton -> LED: %.2f us (requisito < %.0f us)",
                 resp_max / 1000.0, RESP_MAX_US);
        if (resp_max / 1000.0 >= RESP_MAX_US) begin
            $error("Respuesta a botones fuera de requisito");
            errors++;
        end

        if (errors == 0) $display("TB_PIO: PASS");
        else             $display("TB_PIO: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
