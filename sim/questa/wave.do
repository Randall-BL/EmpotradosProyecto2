# Senales de los PIO en el waveform de Questa (origen: Taller 9, scripts/wave.do).
add wave -noupdate -label clk /audio_soc_tb/audio_soc_inst/clk_clk
add wave -noupdate -label reset_n /audio_soc_tb/audio_soc_inst/reset_reset_n
add wave -noupdate -divider "PIO de salida @0x40010"
add wave -noupdate -label pio_out_export -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_out_export_export
add wave -noupdate -label pio_out.chipselect /audio_soc_tb/audio_soc_inst/pio_out/chipselect
add wave -noupdate -label pio_out.write_n /audio_soc_tb/audio_soc_inst/pio_out/write_n
add wave -noupdate -label pio_out.writedata -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_out/writedata
add wave -noupdate -divider "PIO de entrada @0x40020 (forzado 4'hA)"
add wave -noupdate -label pio_in_export -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_in_export_export
add wave -noupdate -label pio_in.readdata -radix hexadecimal /audio_soc_tb/audio_soc_inst/pio_in/readdata
configure wave -namecolwidth 200
configure wave -valuecolwidth 90
configure wave -timelineunits us
