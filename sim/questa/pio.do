# Estimulo y observacion de los PIO en Questa (origen: Taller 9, scripts/pio.do).
# Se ejecuta despues de "source msim_setup.tcl; file_copy; ld_debug".
#   pio_in  : se fuerza a 4'hA (simula 4 interruptores) antes de correr.
#   pio_out : se registra para verlo en el waveform.
log -r /*
force -freeze sim:/audio_soc_tb/audio_soc_inst/pio_in_export_export 4'hA 0
