// Co-simulacion del reproductor completo (IN-07 sin tarjeta; NV-03, NV-13, NV-14).
//
// El Nios V ejecuta sw/niosv/tests/test_player: la logica real de la aplicacion
// (sw/common/src/app.c) con los drivers de botones (IRQ), switches, LEDs y displays,
// y la memoria compartida real con un modelo del HPS que publica 10 pistas cortas.
// Este testbench hace de usuario: pulsa KEY[3:0], mueve SW y lee LEDR y HEX5..HEX4
// (numero de pista).
//
// Verifica:
//   - espera al HPS y muestra la pista 01 al recibir la tabla
//   - anterior/siguiente detenido, con vuelta (01 -> 10 -> 01)
//   - play: LEDR7 y avance automatico por las 10 pistas, DOS vueltas (PR-08)
//   - pausa (LEDR8, la pista no avanza) y reanudar; stop (LEDR7/8 apagados, 00:00)
//   - switches -> filtro en LEDR[2:0] y combinacion invalida en LEDR9 (CD-08)
//   - respuesta boton -> LED < 200 ms (PR-07), sin subdesbordes (LEDR5) y el flujo
//     llega completo y en orden (si no, el programa pone LEDR = 0x3EE)
//
// El reloj de la aplicacion va 100 veces mas rapido (test_player) para que el
// antirrebote (50 ms de la app = 0.5 ms simulados) y la encuesta de switches no alarguen
// la simulacion. Por eso los limites de espera de este tb estan en tiempo REAL simulado
// (us) y la respuesta a botones se mide tambien en tiempo real, que es lo que pide el
// requisito. La pantalla no se dibuja (puerto nulo): la verifica tb_vga.
//
// Uso: scripts/sim-tb.sh player   (termina con "TB_PLAYER: PASS" o "TB_PLAYER: FAIL")
`timescale 1ns / 1ps

module tb_player;
    localparam real APP_MS = 10_000.0;          // 1 ms de la aplicacion, en ns
    localparam real US     = 1_000.0;
    localparam real STEP_US = 20_000.0;         // espera maxima por paso (20 ms reales)

    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;
    logic [3:0] key = 4'hF;
    logic [9:0] sw  = 10'h000;
    wire  [9:0] led;
    wire  [6:0] hex [6];
    wire sda;
    pullup (sda);

    audio_soc dut (
        .clk_clk (clk), .reset_reset_n (rst_n),
        .key_export (key), .sw_export (sw), .led_export (led),
        .audio_clk_clk (), .audio_BCLK (1'b0), .audio_DACLRCK (1'b0), .audio_DACDAT (),
        .av_config_SCLK (), .av_config_SDAT (sda),
        .adc_sclk (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R (), .vga_G (), .vga_B (),
        .hex_hex0 (hex[0]), .hex_hex1 (hex[1]), .hex_hex2 (hex[2]),
        .hex_hex3 (hex[3]), .hex_hex4 (hex[4]), .hex_hex5 (hex[5])
    );

    function automatic int seg(input logic [6:0] s);
        case (s)
        7'b1000000: return 0;  7'b1111001: return 1;  7'b0100100: return 2;
        7'b0110000: return 3;  7'b0011001: return 4;  7'b0010010: return 5;
        7'b0000010: return 6;  7'b1111000: return 7;  7'b0000000: return 8;
        7'b0010000: return 9;  default:    return -1;
        endcase
    endfunction

    // Los displays se pasan como argumentos: un wait() solo es sensible a las senales que
    // aparecen en su expresion, no a las que una funcion lee por dentro.
    function automatic int track_of(input logic [6:0] h5, input logic [6:0] h4);
        int t = seg(h5), u = seg(h4);
        return (t < 0 || u < 0) ? -1 : t * 10 + u;
    endfunction

    function automatic int track_shown();
        return track_of(hex[5], hex[4]);
    endfunction

    function automatic string time_shown();
        return $sformatf("%0d%0d:%0d%0d", seg(hex[3]), seg(hex[2]), seg(hex[1]), seg(hex[0]));
    endfunction

    int  errors = 0;
    real resp_max = 0.0;                        // en us reales

    initial forever @(led or hex[5] or hex[4]) begin
        #1 $display("[%0t] LEDR = 0x%03h  pista %0d", $time, led, track_shown());
    end

    // El programa marca un flujo corrupto con 0x3EE: se reporta en cuanto pasa.
    initial begin
        wait (led === 10'h3EE);
        $error("[%0t] el sumidero recibio un cuadro fuera de orden (LEDR = 0x3EE)", $time);
        errors++;
    end

    task automatic check(input bit ok, input string what);
        if (ok) $display("[%0t] OK    %s", $time, what);
        else begin
            $error("[%0t] FALLO %s (LEDR = 0x%03h, pista %0d)", $time, what, led, track_shown());
            errors++;
        end
    endtask

    // Espera hasta timeout_us (tiempo real simulado) a que la pista mostrada sea n.
    task automatic wait_track(input int n, input real timeout_us, input string what);
        fork
            wait (track_of(hex[5], hex[4]) == n);
            #(timeout_us * US);
        join_any
        disable fork;
        check(track_shown() == n, $sformatf("%s: pista %02d", what, n));
    endtask

    task automatic wait_led(input logic [9:0] mask, input logic [9:0] value,
                            input real timeout_us, input string what);
        fork
            wait ((led & mask) === value);
            #(timeout_us * US);
        join_any
        disable fork;
        check((led & mask) === value, what);
    endtask

    // Pulsa KEY[k] y mide (en us reales) hasta que cambia LEDR6 (boton aceptado). Lo
    // suelta despues y espera 60 ms de la app: fuera de la ventana de antirrebote.
    task automatic press(input int k, input string what);
        logic led6_prev = led[6];
        realtime t0;
        real ms;
        key[k] = 1'b0;
        t0 = $realtime;
        fork
            wait (led[6] !== led6_prev);
            #(STEP_US * US);
        join_any
        disable fork;
        if (led[6] === led6_prev) begin
            $error("[%0t] FALLO %s: el boton no fue aceptado", $time, what); errors++;
        end else begin
            ms = ($realtime - t0) / US;
            if (ms > resp_max) resp_max = ms;
            $display("[%0t] OK    %s: aceptado en %.1f us", $time, what, ms);
        end
        #(10.0 * APP_MS);
        key[k] = 1'b1;
        #(60.0 * APP_MS);                       // fuera de la ventana de antirrebote
    endtask

    initial begin
        int changes, last;
        $display("TB_PLAYER: inicio");
        #200 rst_n = 1'b1;

        wait_track(1, 50_000.0, "tabla del HPS recibida");
        check((led & 10'h180) == 0, "arranca detenido (LEDR7/8 apagados)");

        press(3, "KEY3 siguiente (detenido)");  wait_track(2, STEP_US, "siguiente");
        press(2, "KEY2 anterior");              wait_track(1, STEP_US, "anterior");
        press(2, "KEY2 anterior desde la 01");  wait_track(10, STEP_US, "vuelta hacia atras");
        press(3, "KEY3 siguiente desde la 10"); wait_track(1, STEP_US, "vuelta hacia adelante");
        check((led & 10'h180) == 0, "navegar detenido no reproduce");

        // Play y dos vueltas completas con avance automatico.
        press(0, "KEY0 play");
        wait_led(10'h080, 10'h080, STEP_US, "reproduciendo (LEDR7)");
        changes = 0;
        last = track_shown();
        fork
            begin
                while (changes < 20) begin
                    @(hex[4] or hex[5]);
                    #1;
                    if (track_shown() >= 0 && track_shown() != last) begin
                        if (track_shown() != (last % 10) + 1) begin
                            $error("[%0t] avance de %0d a %0d", $time, last, track_shown());
                            errors++;
                        end
                        last = track_shown();
                        changes++;
                    end
                end
            end
            #(80_000.0 * US);
        join_any
        disable fork;
        check(changes >= 20, $sformatf("avance automatico: %0d cambios de pista (2 vueltas)", changes));

        press(0, "KEY0 pausa");
        wait_led(10'h180, 10'h100, STEP_US, "pausado (LEDR8)");
        last = track_shown();
        #(5_000.0 * US);
        check(track_shown() == last, "en pausa la pista no avanza (5 ms)");
        press(0, "KEY0 reanudar");
        wait_led(10'h180, 10'h080, STEP_US, "reanuda (LEDR7)");

        press(1, "KEY1 stop");
        wait_led(10'h180, 10'h000, STEP_US, "detenido (LEDR7/8 apagados)");
        check(time_shown() == "00:00", $sformatf("tiempo en 00:00 (%s)", time_shown()));

        sw = 10'h001;  wait_led(10'h007, 10'h001, STEP_US, "SW0 -> pasa-bajos (LEDR0)");
        sw = 10'h002;  wait_led(10'h007, 10'h002, STEP_US, "SW1 -> pasa-banda (LEDR1)");
        sw = 10'h004;  wait_led(10'h007, 10'h004, STEP_US, "SW2 -> reverberacion (LEDR2)");
        sw = 10'h003;  wait_led(10'h207, 10'h200, STEP_US, "SW0+SW1 invalido -> bypass y LEDR9");
        sw = 10'h000;  wait_led(10'h207, 10'h000, STEP_US, "SW abajo -> bypass sin alerta");

        check(led[5] === 1'b0, "sin subdesbordes (LEDR5 apagado)");
        $display("Respuesta maxima boton -> LED: %.1f us reales (requisito < 200 ms)", resp_max);
        check(resp_max < 200_000.0, "respuesta a botones < 200 ms");

        if (errors == 0) $display("TB_PLAYER: PASS");
        else             $display("TB_PLAYER: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
