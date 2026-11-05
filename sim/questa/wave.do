# Senales de los PIO en el waveform de Questa (origen: Taller 9, scripts/wave.do).
add wave -noupdate -label clk /audio_soc_tb/audio_soc_inst/clk_clk
add wave -noupdate -label reset_n /audio_soc_tb/audio_soc_inst/reset_reset_n
add wave -noupdate -divider "PIO de LEDR @0x40010"
add wave -noupdate -label led_export -radix hexadecimal /audio_soc_tb/audio_soc_inst/led_export
add wave -noupdate -label pio_led.chipselect /audio_soc_tb/audio_soc_inst/pio_led/chipselect
add wave -noupdate -label pio_led.write_n /audio_soc_tb/audio_soc_inst/pio_led/write_n
add wave -noupdate -label pio_led.writedata -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_led/writedata
add wave -noupdate -divider "PIO de SW @0x40030 (forzado 10h00A)"
add wave -noupdate -label sw_export -radix hexadecimal /audio_soc_tb/audio_soc_inst/sw_export
add wave -noupdate -label pio_sw.readdata -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_sw/readdata
configure wave -namecolwidth 200
configure wave -valuecolwidth 90
configure wave -timelineunits us
