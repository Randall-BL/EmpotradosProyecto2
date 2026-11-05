# Deja la GUI de Questa lista para la captura de evidencia (origen: Taller 9,
# scripts/evidence.do). Ajustar el zoom a la ventana de interes: los tiempos
# de abajo corresponden a las escrituras a pio_out del programa de prueba
# (2.55-2.85 ms). El transcript queda abajo con toda la salida del programa.
quietly catch {noview objects}
quietly catch {noview processes}
quietly catch {noview sim}
quietly catch {wm geometry . 1366x390+0+0}
quietly update
quietly view -undock -x 0 -y 430 -width 1366 -height 300 transcript
quietly wave zoom range 2550us 2850us
