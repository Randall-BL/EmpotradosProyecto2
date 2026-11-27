# Restricciones de tiempo del reproductor de audio (HW-20).

# ---- Relojes ----
# CLOCK_50: reloj del sistema (Nios V, buses, IP de audio, ADC, Character Buffer).
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]

# Reloj de JTAG (JTAG UART y depuracion del Nios V).
create_clock -name altera_reserved_tck -period 33.333 [get_ports {altera_reserved_tck}]
set_clock_groups -asynchronous -group [get_clocks {altera_reserved_tck}]
set_false_path -from [get_ports {altera_reserved_tdi altera_reserved_tms}]
set_false_path -to   [get_ports {altera_reserved_tdo}]

# Relojes de los PLL: 12.288 MHz para el codec y 25 MHz para el VGA.
derive_pll_clocks -create_base_clocks
derive_clock_uncertainty

# Nota: el Timing Analyzer lista rst_controller|r_sync_rst como "reloj sin
# restringir" porque llega al pin rst de los PLL de audio y video, que el atomo
# del PLL declara como entrada de tipo reloj. Es un reset asincrono: no hay
# rutas de datos que analizar.

# ---- Entradas y salidas asincronas ----
# Botones y switches los muestrea el PIO (y el software hace antirrebote); los
# LEDs y displays no tienen requisito de tiempo frente a un reloj externo.
set_false_path -from [get_ports {KEY[*] SW[*]}]
set_false_path -to   [get_ports {LEDR[*] HEX0[*] HEX1[*] HEX2[*] HEX3[*] HEX4[*] HEX5[*]}]

# Codec: el IP de Audio sincroniza BCLK y DACLRCK con dos flip-flops y saca
# DACDAT tras detectar el flanco (altera_up_clock_edge), es decir, trata la
# interfaz como asincrona. AUD_XCK es la salida del PLL hacia el codec.
set_false_path -from [get_ports {AUD_BCLK AUD_DACLRCK AUD_ADCLRCK AUD_ADCDAT}]
set_false_path -to   [get_ports {AUD_DACDAT AUD_XCK}]

# I2C del codec (100 kHz generado por divisor) y SPI del ADC (12.5 MHz generado
# por logica): ambos se manejan con registros a 50 MHz, sin reloj externo.
set_false_path -to   [get_ports {FPGA_I2C_SCLK FPGA_I2C_SDAT}]
set_false_path -from [get_ports {FPGA_I2C_SDAT}]
set_false_path -to   [get_ports {ADC_CONVST ADC_DIN ADC_SCLK}]
set_false_path -from [get_ports {ADC_DOUT}]

# VGA: datos y sincronismos salen registrados con el reloj de 25 MHz, que se
# entrega al DAC por VGA_CLK; el DAC los toma con amplio margen (40 ns).
set_false_path -to   [get_ports {VGA_R[*] VGA_G[*] VGA_B[*] VGA_HS VGA_VS VGA_BLANK_N VGA_SYNC_N VGA_CLK}]
