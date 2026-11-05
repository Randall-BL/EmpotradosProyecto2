# Estimulo y observacion de los PIO en Questa (origen: Taller 9, scripts/pio.do).
# Se ejecuta despues de "source msim_setup.tcl; file_copy; ld_debug".
#   sw      : se fuerza a 10'h00A (simula SW) antes de correr.
#   led     : se registra para verlo en el waveform.
log -r /*
force -freeze sim:/audio_soc_tb/audio_soc_inst/sw_export 10'h00A 0
