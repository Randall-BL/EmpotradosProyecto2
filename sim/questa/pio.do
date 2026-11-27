# Registro de senales para el waveform (origen: Taller 9, scripts/pio.do).
# Se ejecuta despues de elaborar sim/questa/tb_smoke.sv; el estimulo (SW = 10'h00A)
# ya lo pone el testbench, asi que aqui no se fuerza nada.
# Registra recursivamente todas las senales del diseno para poder verlas despues.
log -r /*
