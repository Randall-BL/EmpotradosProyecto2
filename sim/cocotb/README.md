# Testbenches cocotb del hardware propio

Verifican cada módulo HDL propio aislado, solo por su interfaz Avalon-MM, con cocotb 1.9
e Icarus Verilog 12 dentro del contenedor. Son los mismos casos que los testbenches de
Questa (`sim/questa/rtl/`), pero corren en la CI en cada PR.

```bash
make docker-sim          # o, dentro del contenedor: make sim
```

| Carpeta | Módulo | Pruebas |
|---|---|---|
| `humo/` | `contador.v` (solo de prueba) | Comprueba la cadena cocotb + Icarus |
| `filtro_audio/` | `filtro_audio` + `fir_core` | `test_registros`: reset, lectura/escritura, límites, solo lectura, coeficientes, CLEAR y `waitrequest`. `test_filtro`: impulso, escalón, senos en banda de paso y de rechazo de los dos bancos, saturación, bypass y los 8 vectores dorados de `sim/vectors/` |
| `seg7_time/` | `seg7_time` | Tiempos de 0 a 8 191 s, saturación a 99:59, canción, apagado y STATUS |

La referencia del filtro es `FirModel` de `scripts/design_fir.py`, el modelo bit a bit
del datapath con los mismos coeficientes Q1.15 que se cargan en el módulo, así que la
salida se compara con 0 LSB de tolerancia. Las comprobaciones contra la matemática ideal
(respuesta al impulso, valor final del escalón) usan ±1 LSB: el datapath redondea una
sola vez por etapa (CD-14).

`avalon.py` es el maestro Avalon-MM compartido: mantiene el pedido mientras el esclavo
levanta `waitrequest` y, sin `waitrequest`, lee con latencia fija de 1 ciclo. Los
resultados (JUnit) y las formas de onda quedan en `build/sim/cocotb/<módulo>/`.
