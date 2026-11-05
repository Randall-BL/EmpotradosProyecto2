# Senales de los PIO en el waveform de Questa (origen: Taller 9, scripts/wave.do).
add wave -noupdate -label clk /tb_smoke/dut/clk_clk
add wave -noupdate -label reset_n /tb_smoke/dut/reset_reset_n
add wave -noupdate -divider "PIO de LEDR @0x40010"
add wave -noupdate -label led_export -radix hexadecimal /tb_smoke/dut/led_export
add wave -noupdate -label pio_led.chipselect /tb_smoke/dut/pio_led/chipselect
add wave -noupdate -label pio_led.write_n /tb_smoke/dut/pio_led/write_n
add wave -noupdate -label pio_led.writedata -radix hexadecimal /tb_smoke/dut/pio_led/writedata
add wave -noupdate -divider "PIO de SW @0x40030 (tb pone 10h00A)"
add wave -noupdate -label sw_export -radix hexadecimal /tb_smoke/dut/sw_export
add wave -noupdate -label pio_sw.readdata -radix hexadecimal /tb_smoke/dut/pio_sw/readdata
configure wave -namecolwidth 200
configure wave -valuecolwidth 90
configure wave -timelineunits us
