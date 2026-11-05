// Testbench autoverificable de la configuracion del codec (HW-07, NV-07, PR-05).
//
// El Nios V ejecuta sw/niosv/tests/test_codec: codec_init() espera la
// autoinicializacion del IP Audio and Video Config y quita la de-enfasis, y
// luego se prueba codec_mute(). wm8731_model hace de esclavo I2C (0x34) y
// registra cada escritura.
//
// Verifica los registros que fija CD-07 (maestro, Left Justified, 16 bits,
// 48 kHz con MCLK 12.288 MHz, codec activo, todo encendido), la correccion de
// R5 y el orden: R9 (activar) al final de la autoinicializacion, y despues las
// escrituras del software (R5 = 0x000, 0x008, 0x000).
//
// La autoinicializacion del IP para la DE1-SoC son 57 transferencias I2C a ~24 kHz
// (~1.19 ms cada una): 10 al codec y el resto al decodificador de video ADV7180,
// que comparte el bus. AIS llega a los ~68 ms; el software escribe despues.
//
// Uso: scripts/sim-tb.sh codec   (lento: ~75 ms simulados, unos 30 min)
//      ELAB_OPTS=-gFAST=1 scripts/sim-tb.sh codec   (depuracion, ~15 ms: al terminar
//      las 10 escrituras del codec salta las del ADV7180 adelantando el puntero de la
//      ROM de autoinicializacion; no sirve como evidencia)
`timescale 1ns / 1ps

module tb_codec #(
    parameter bit FAST = 0
);
    logic clk = 1'b0, rst_n = 1'b0;
    always #10 clk = ~clk;

    wire [9:0] led;
    wire scl, sda;
    pullup (sda);

    audio_soc dut (
        .clk_clk        (clk),
        .reset_reset_n  (rst_n),
        .key_export     (4'hF),
        .sw_export      (10'h000),
        .led_export     (led),
        .audio_clk_clk  (),
        .audio_BCLK     (1'b0),
        .audio_DACLRCK  (1'b0),
        .audio_DACDAT   (),
        .av_config_SCLK (scl),
        .av_config_SDAT (sda),
        .adc_sclk       (), .adc_cs_n (), .adc_din (), .adc_dout (1'b0),
        .vga_CLK        (), .vga_HS (), .vga_VS (), .vga_BLANK (), .vga_SYNC (),
        .vga_R          (), .vga_G  (), .vga_B  ()
    );

    wm8731_model codec (
        .enable (1'b0), .bclk (), .daclrck (), .dacdat (1'b0),
        .left (), .right (), .frame_valid (),
        .scl (scl), .sda (sda)
    );

    int errors = 0;
    typedef struct { logic [6:0] r; logic [8:0] d; } wr_t;
    wr_t log_q[$];

    always @(posedge codec.reg_valid) begin
        log_q.push_back('{codec.reg_addr, codec.reg_data});
        $display("[%0t] I2C: R%0d <= 0x%03h", $time, codec.reg_addr, codec.reg_data);
        if (FAST && codec.reg_addr == 9 && log_q.size() == 10) begin
            #1us;
            dut.av_config.AV_Config_Auto_Init.rom_address = dut.av_config.AV_Config_Auto_Init.ROM_SIZE - 1;
            $display("[%0t] FAST: se saltan las escrituras al ADV7180", $time);
            codec.debug = 1;
        end
    end
    always @(led) $display("[%0t] LEDR = 0x%03h", $time, led);
    always @(posedge dut.av_config.auto_init_complete) $display("[%0t] AIS: autoinicializacion completa", $time);

    task automatic check_reg(input int r, input logic [8:0] exp, input string what);
        if (codec.regs[r] !== exp) begin
            $error("R%0d = 0x%03h, se esperaba 0x%03h (%s)", r, codec.regs[r], exp, what);
            errors++;
        end else
            $display("OK    R%0d = 0x%03h  %s", r, codec.regs[r], what);
    endtask

    initial begin
        $display("TB_CODEC: inicio");
        #200 rst_n = 1'b1;
        fork
            wait (led == 10'h0C0 || led[9:4] == 6'h0E);
            begin #150ms; $error("timeout esperando al software"); errors++; end
        join_any
        disable fork;
        if (led !== 10'h0C0) begin
            $error("el software reporto error: LEDR = 0x%03h", led);
            errors++;
        end
        // El IP da la transferencia por terminada (RDY = 1) al entrar al bit de STOP,
        // ~20 us antes de que el STOP aparezca en el bus: se espera a que llegue.
        #200us;

        // Valores finales (R5 ya corregido por el software).
        check_reg(7, 9'h041, "formato: maestro, Left Justified, 16 bits");
        check_reg(8, 9'h000, "muestreo: modo normal, BOSR 256fs, 48 kHz con 12.288 MHz");
        check_reg(6, 9'h000, "potencia: todos los bloques encendidos");
        check_reg(9, 9'h001, "interfaz digital activa");
        check_reg(5, 9'h000, "DAC sin de-enfasis ni silencio (corregido por codec_init)");
        check_reg(4, 9'h092 & 9'h1FF, "camino analogico: DAC seleccionado");

        // Orden: ..., R9 (fin de la autoinicializacion), R5=0x000, R5=0x008, R5=0x000.
        if (log_q.size() < 4) begin
            $error("solo %0d escrituras I2C", log_q.size()); errors++;
        end else begin
            automatic int n = log_q.size();
            if (!(log_q[n-4].r == 9 && log_q[n-3].r == 5 && log_q[n-3].d == 9'h000 &&
                  log_q[n-2].r == 5 && log_q[n-2].d == 9'h008 &&
                  log_q[n-1].r == 5 && log_q[n-1].d == 9'h000)) begin
                $error("orden de escrituras inesperado al final de la secuencia"); errors++;
            end else
                $display("OK    secuencia: autoinicializacion -> R5 sin de-enfasis -> silencio -> sonido");
        end
        $display("Escrituras I2C recibidas: %0d al codec, %0d a otros dispositivos del bus (ADV7180)",
                 log_q.size(), codec.other_writes);
        $display("Autoinicializacion + software terminaron a los %.1f ms", $realtime / 1.0e6);

        if (errors == 0) $display("TB_CODEC: PASS");
        else             $display("TB_CODEC: FAIL (%0d errores)", errors);
        $finish;
    end
endmodule
