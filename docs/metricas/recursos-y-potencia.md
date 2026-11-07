# Análisis de recursos y consumo (RP-01..RP-05, DOC-04)

Rubro "Análisis de recursos y consumo" (§3-REC p.6, 3 % de la nota). Datos de la
compilación de Quartus Prime Lite 25.1std para el 5CSEMA5F31C6, regenerables con:

```bash
scripts/build-quartus.sh
python3 scripts/report-resources.py            # RP-01 (y --entity <modulo> para RP-02)
scripts/report-power.sh                        # RP-03
```

> Estado (7 nov): plataforma de A, módulo de filtro propio y `seg7_time` (B) integrados.
> Falta el HPS (HW-05, D); cuando entre se vuelven a correr los scripts.

## RP-01 — Utilización del sistema completo

| Recurso | Uso | Disponible | % |
|---|---|---|---|
| Lógica (ALMs) | 3 773 | 32 070 | 12 % |
| Registros | 5 304 | — | — |
| Bits de memoria embebida | 1 661 056 | 4 065 280 | 41 % |
| Bloques M10K | 210 | 397 | 53 % |
| Bloques DSP | 4 | 87 | 5 % |
| PLL | 2 | 6 | 33 % |
| Pines | 108 | 457 | 24 % |

Por componente (ALMs / M10K): `cpu` 1 860 / 2 · `mm_interconnect_0` 762 / 0 · `adc` 127 / 0 ·
`av_config` 111 / 0 · `audio` 79 / 2 · `juart` 60 / 2 · `char_buf` 52 / 8 ·
`video_fifo` 38 / 1 · `ram` 38 / 128 · `vga` 35 / 0 · PIOs 45 / 0 · `shm` 2 / 64. Tabla
completa y justificación de cada uno: [docs/hw/componentes-soc.md](../hw/componentes-soc.md).

## RP-02 — Utilización del módulo propio

`python3 scripts/report-resources.py` (fila de cada instancia) y el reporte de potencia por
jerarquía de `quartus_pow`:

| Módulo | ALMs | Registros | Bits de memoria | M10K | DSP | Potencia |
|---|---|---|---|---|---|---|
| `filtro` (`filtro_audio` + `fir_core`) | 250 | 355 | 12 288 | 3 | **4** | 4.42 mW |
| `seg7` (`seg7_time`) | 115 | 100 | 0 | 0 | 0 | < 0.1 mW |
| Referencia: `cpu` (Nios V/m) | 1 860 | 2 475 | 2 048 | 2 | 0 | 15.31 mW |

Los 4 DSP son los dos multiplicadores 16 × 16 de los MAC (uno por canal) y los dos 16 × 17
de la etapa de ganancia. Los 3 M10K son el banco de coeficientes (2 × 128 × 16 bits) y las
dos líneas de retardo (128 × 16 bits cada una).

## RP-03 — Potencia del sistema completo

Power Analyzer (`quartus_pow`), estimación *vectorless* con tasa de conmutación de 12.5 %
en las entradas (valor por defecto de Quartus), modelos de potencia finales:

| Componente | Potencia |
|---|---|
| Total | **557.0 mW** |
| Núcleo, estática | 421.7 mW |
| Núcleo, dinámica | 99.3 mW |
| E/S | 36.0 mW |

El hardware propio sumó 9.2 mW: 4.6 mW de dinámica del núcleo (el filtro estima 4.42 mW)
y 4.5 mW de E/S por los 42 pines de los displays, que antes estaban fijos.

Confianza "baja" según Quartus porque no hay vectores de simulación. La potencia
**estática domina (77 %)**: es la fuga del dispositivo completo y casi no depende del
diseño. La dinámica sí depende de las decisiones de partición (siguiente sección). Para
subir la confianza se puede pasar un VCD de una simulación con `VCD=... scripts/report-power.sh`.

## RP-05 — Efecto de la partición HW/SW en recursos y potencia

| Decisión | Alternativa descartada | Efecto medido o estimado |
|---|---|---|
| Código y buffers del Nios V en RAM on-chip, sin SDRAM ([CD-06](../decisiones/CD-06.md)) | SDRAM de 64 MB | Cuesta 192 de los 207 M10K (93 % de la memoria usada), pero evita el controlador SDRAM, su PLL y ~40 pines con conmutación constante (refresco). La memoria es el recurso más cargado (52 % de los M10K): limita cuánto puede crecer `shm` (128 KB = 64 M10K más) y deja ~190 M10K para el filtro propio. |
| Remuestreo de 8/16/44.1 kHz a 48 kHz en el HPS ([CD-07](../decisiones/CD-07.md)) | Remuestrear en el Nios V o reconfigurar el códec | 0 recursos de FPGA: el ARM tiene multiplicador y FPU. En el Nios V/m (RV32I, sin `mul`) costaría una llamada a `__mulsi3` por muestra y canal. Con un solo fs, el filtro de B usa un solo juego de coeficientes. |
| Volumen por escalado en software (SC-11) | Multiplicador en hardware antes del IP de Audio (CD-05, B) | 0 DSP y 0 ALMs; a cambio, 2 multiplicaciones por software por par de muestras (96 000/s). Si el lazo principal no alcanza, el multiplicador en HW cuesta 1 bloque DSP de los 87 libres. |
| Visualización con IPs del University Program ([CD-09](../decisiones/CD-09.md)) | Controlador VGA de texto propio | 126 ALMs, 9 M10K y 1 PLL para toda la cadena de video; un controlador propio no ahorraría mucho y costaría el desarrollo y la verificación. |
| ADC con 2 canales (HW-08) | 1 canal | +74 ALMs (de 53 a 127) para que CH0 se actualice también con el LTC2308 (el IP guarda cada resultado en el registro del canal anterior). Es costo de compatibilidad, no de función. |
| Nios V/m con pipeline (HW-03) | Sin pipeline | El procesador es el bloque más grande (1 860 ALMs, 57 % del SoC). El pipeline cuesta lógica pero baja el CPI; medido en simulación, un acceso a memoria en un lazo cuesta ~30 ciclos, así que sin pipeline el lazo de audio a 48 kHz no tendría margen. |
| Filtro FIR en hardware (RP-04, [CD-12](../decisiones/CD-12.md)) | El mismo FIR en el Nios V | Ver RP-04: 250 ALMs, 4 DSP, 3 M10K y 4.4 mW frente a 16 362 ciclos por par en software (15.7 veces el procesador). |

## RP-04 — Costo de mover un filtro de software a hardware

Mismo filtro (FIR de 127 taps, Q1.15, redondeo y saturación idénticos, verificado bit a bit)
en las dos particiones. Ciclos del Nios V medidos con `tb_ciclos` (`-O2`, contador
`mtime`); recursos y potencia de Quartus:

| | Software (Nios V/m, `dsp_fir`) | Hardware (`filtro_audio`) |
|---|---|---|
| Ciclos por par estéreo | **16 362** (8 181 por canal) | 136 de latencia; el CPU solo hace 2 accesos al bus (~10–20 ciclos con *pipelining*) |
| Uso del procesador a 48 kHz | **1 572 %** (imposible) | **1–2 %** |
| Lógica | 0 | 250 ALMs (0.8 % del dispositivo) |
| Multiplicadores | 0 (`__mulsi3` en software) | 4 bloques DSP (de 87) |
| Memoria | ~0.5 KB de RAM del Nios V | 3 M10K (de 397) |
| Potencia | La del CPU, que no daría abasto | 4.42 mW |

Lo mismo pasa con el volumen (CD-05): 542 ciclos por par en software (52 % del CPU) frente
a 2 DSP dentro del módulo. En cambio la reverberación (54 ciclos por muestra, solo sumas y
desplazamientos) cabe en el Nios V y se queda en software (CD-12).

## Conclusión

Mover el FIR y la ganancia a hardware cuesta **menos del 1 % de la lógica, 4 de 87 DSP,
3 de 397 M10K y 4.4 mW**, y es lo que hace posible el sistema: sin multiplicador en el
Nios V/m, el filtro en software necesitaría 16 veces más procesador del que hay. Con todo
integrado el SoC usa **12 % de la lógica y 5 % de los DSP**; el recurso que hay que cuidar
es la **memoria embebida** (53 % de los M10K), sobre todo si el presupuesto de latencia
(CD-03) pide un buffer compartido más grande.
