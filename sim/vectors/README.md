# Vectores dorados del filtro FIR (generados por scripts/design_fir.py)

Primera linea: `banco taps ganancia bypass muestras`. Luego, por muestra:
`entrada salida` en hex de 32 bits = `{R[15:0], L[15:0]}`.

| Archivo | Caso | Satura |
|---|---|---|
| `fir_impulso.txt` | impulso en L (salida = coeficientes) | no |
| `fir_escalon.txt` | escalon de 0.5 (valor final = 0.5 * suma de coeficientes) | no |
| `fir_senos.txt` | L: 500 Hz (pasa), R: 8 kHz (se rechaza) | no |
| `fir_saturacion.txt` | entrada con el signo de cada coeficiente: satura | si |
| `fir_ruido_banco1.txt` | ruido blanco por el pasa-altos (banco 1) | no |
| `fir_bypass_gain.txt` | bypass con ganancia 0.5 | no |
| `fir_taps16.txt` | solo 16 taps del banco 0, ruido | no |
| `fir_ganancia_alta.txt` | ganancia 1.5 sobre el pasa-bajos: satura la etapa de ganancia | si |
