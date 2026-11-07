# Herramientas de ingeniería (DOC-05)

Rubro "Herramientas de ingeniería" (§5 p.8, 3 %): herramientas usadas, y todo modelo,
ecuación, script y herramienta creados o modificados por el grupo. Estado al 7 de noviembre
de 2026 (plataforma de A y hardware propio de B).

## Herramientas usadas

| Herramienta | Versión | Para qué |
|---|---|---|
| Quartus Prime Lite | 25.1std | Síntesis, *fitter*, ensamblador, Timing Analyzer, Power Analyzer, actualización de la RAM en el `.sof` |
| Platform Designer (`qsys-script`, `qsys-generate`) | 25.1std | SoC descrito en Tcl (`hw/soc/build_system.tcl`), componentes propios (`hw/rtl/*_hw.tcl`) |
| Nios V / RiscFree (`niosv-bsp`, `niosv-app`, `riscv32-unknown-elf-gcc` 13.2) | 25.1std | BSP, compilación del software, `elf2hex` |
| Questa Altera Starter FPGA Edition | 2025.2 | Simulación del SoC completo y de los módulos propios aislados |
| Python 3 + NumPy, SciPy, Matplotlib | 3.13 / 2.2 / 1.14 / 3.8 | Diseño de filtros, cuantización, vectores dorados, gráficos, reportes |
| GCC del PC | — | Pruebas de la lógica portable (`make -C tests`) |
| Git + GitHub (issues, *milestones*, Projects, PR) | — | Flujo de trabajo (CONTRIBUTING.md) |
| Docker + cocotb + Icarus | — | *Pendiente (DK-*, C)*: los testbenches reutilizarán `sim/vectors/` |

## Modelos y ecuaciones

| Modelo | Ecuación / definición | Dónde | Usado en |
|---|---|---|---|
| FIR | `y[n] = Σ_{k<N} h[k] · x[n−k]`, N = 127, ventana de Kaiser β = 6 | `scripts/design_fir.py` | Banco 0: pasa-bajos 1 kHz; banco 1: pasa-banda 1–4 kHz |
| Punto fijo del FIR | Q1.15; acumulador de 40 bits; `y = sat16((acc + 2^14) >> 15)` | CD-14, `fir_core.v`, `dsp.c`, `FirModel` | Modelo bit a bit compartido por HDL, C y vectores |
| Ganancia (volumen) | `z = sat16((y · g + 2^14) >> 15)`, g Q1.15 sin signo (0–2.0) | CD-05, `fir_core.v`, `dsp_gain` | Etapa de ganancia del módulo propio |
| Curva de volumen | 32 escalones de 2 dB: `g_i = 32768 · 10^(−(31−i)·2/20)`, `g_0 = 0`; histéresis de 24 cuentas | `scripts/gen_volume_table.py`, `volume.c` | Potenciómetro → `GAIN` |
| Reverberación | `y[n] = x[n]/2 + y[n−D]/2`, D = 2 400 (50 ms) | `dsp_reverb` (CD-12) | Tercer filtro, en el Nios V |
| Tono de prueba | Acumulador de fase: `φ += f·2^32/fs`, `s = sen[φ >> 22]`, tabla de 1 024 puntos | `scripts/gen_sine_table.py`, `tone.c` | IN-02 (SNR 48.9 dB) |
| Presupuesto de ciclos | `f_clk / f_s = 50 MHz / 48 kHz = 1 041` ciclos por par estéreo | CD-04, CD-05, CD-12 | Partición HW/SW |
| Latencia del filtro | `N + 9` ciclos (136 con 127 taps = 2.72 µs) | `tb_filtro_audio` | Cumple < 100 ms de cambio de filtro |
| Tiempo MM:SS | Restas sucesivas de 60 y de 10 (sin divisores) | `seg7_time.v`, `screen.c` | Displays y pantalla |
| Memoria compartida | 64 KB / (48 000 × 2 × 2 B/s) = 341 ms de audio | CD-06 | Tamaño de `shm` |
| Muestreo del códec | MCLK = 256 · fs = 12.288 MHz; BCLK = MCLK/4 | CD-07 | Audio PLL y códec a 48 kHz |

## Scripts creados o modificados

| Script | Qué hace |
|---|---|
| `scripts/env.sh` *(modificado del Taller 9)* | Entorno de Quartus/Questa; rutas por máquina en `env.local.sh`; `cmake` de RiscFree |
| `scripts/build-soc.sh` *(modificado)* | `.qsys` desde el Tcl con los componentes propios, RTL de síntesis y modelo de simulación |
| `scripts/build-niosv.sh` *(modificado)* | BSP sin HAL de los IP (`sw/niosv/bsp.tcl`), `-O2`, un directorio por programa (`APP=`) |
| `scripts/build-quartus.sh` | Compilación completa y resumen de recursos y slack por reloj |
| `scripts/update-sw.sh` | Cambia el software dentro del `.sof` sin recompilar (~20 s) |
| `scripts/program.sh` | Programa la FPGA por JTAG |
| `scripts/sim-soc.sh` *(modificado)* | Prueba de humo del SoC con `tb_smoke` |
| `scripts/sim-tb.sh` | Testbench autoverificable del SoC completo, en paralelo |
| `scripts/sim-rtl.sh` | Testbench aislado de un módulo HDL propio |
| `scripts/test-all.sh` | Toda la verificación con resumen PASS/FAIL |
| `scripts/export-results.sh` | Copia los resultados a `docs/verificacion/resultados/` |
| `scripts/design_fir.py` | Diseño, cuantización y análisis de error de los FIR; exporta coeficientes y vectores dorados |
| `scripts/gen_volume_table.py`, `scripts/gen_sine_table.py` | Tablas de volumen y de seno |
| `scripts/expand-mif.py` | Corrige el `.mif` de la fuente del Character Buffer para la simulación |
| `scripts/plot_vectors.py`, `scripts/pbm2png.py` | Figuras de evidencia (formas de onda, captura VGA) |
| `scripts/report-resources.py`, `report-power.sh`, `report-memory.sh`, `report-memory-map.py` | Recursos por componente, potencia, uso de RAM y mapa de memoria exportado |

Material reutilizado del Taller 9 y sus cambios: [reutilizado-taller9.md](reutilizado-taller9.md).

## Modelos de verificación

| Modelo | Qué modela | Base |
|---|---|---|
| `sim/questa/wm8731_model.sv` | Códec maestro Left Justified a 48 kHz y esclavo I²C | Datasheet del WM8731 |
| `sim/questa/adc_model.sv` | AD7928 y LTC2308 (las dos revisiones de la placa) | Datasheets de ambos chips |
| `FirModel` (`design_fir.py`) y `dsp.c` | Datapath del filtro propio, bit a bit | CD-14 |
