# Análisis de recursos y consumo (RP-01..RP-05, DOC-04)

Rubro "Análisis de recursos y consumo" (§3-REC p.6, 3 % de la nota). Datos de la
compilación de Quartus Prime Lite 25.1std para el 5CSEMA5F31C6, regenerables con:

```bash
scripts/build-quartus.sh
python3 scripts/report-resources.py            # RP-01 (y --entity <modulo> para RP-02)
scripts/report-power.sh                        # RP-03
```

> Estado: sistema de la plataforma (persona A) completo. Faltan en el SoC el módulo de
> filtro propio (HW-17, B), `seg7_time` (HW-10, B) y el HPS (HW-05, D); cuando entren se
> vuelven a correr los scripts y se completan RP-02 y RP-04.

## RP-01 — Utilización del sistema completo

| Recurso | Uso | Disponible | % |
|---|---|---|---|
| Lógica (ALMs) | 3 292 | 32 070 | 10 % |
| Registros | 4 671 | — | — |
| Bits de memoria embebida | 1 648 768 | 4 065 280 | 41 % |
| Bloques M10K | 207 | 397 | 52 % |
| Bloques DSP | 0 | 87 | 0 % |
| PLL | 2 | 6 | 33 % |
| Pines | 108 | 457 | 24 % |

Por componente (ALMs / M10K): `cpu` 1 860 / 2 · `mm_interconnect_0` 762 / 0 · `adc` 127 / 0 ·
`av_config` 111 / 0 · `audio` 79 / 2 · `juart` 60 / 2 · `char_buf` 52 / 8 ·
`video_fifo` 38 / 1 · `ram` 38 / 128 · `vga` 35 / 0 · PIOs 45 / 0 · `shm` 2 / 64. Tabla
completa y justificación de cada uno: [docs/hw/componentes-soc.md](../hw/componentes-soc.md).

## RP-02 — Utilización del módulo propio *(pendiente: HW-17)*

`python3 scripts/report-resources.py --entity <instancia del filtro>` da el detalle por
sub-bloque (registros, multiplicadores, memoria de coeficientes).

## RP-03 — Potencia del sistema completo

Power Analyzer (`quartus_pow`), estimación *vectorless* con tasa de conmutación de 12.5 %
en las entradas (valor por defecto de Quartus), modelos de potencia finales:

| Componente | Potencia |
|---|---|
| Total | **547.8 mW** |
| Núcleo, estática | 421.6 mW |
| Núcleo, dinámica | 94.7 mW |
| E/S | 31.5 mW |

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
| Filtro DSP propio en hardware *(pendiente, RP-04 de B)* | Filtro en software | Se completa con los recursos del módulo (RP-02) frente a los ciclos por muestra del mismo filtro en software medidos con `mcycle`. |

Conclusión parcial: con la plataforma completa el SoC usa **10 % de la lógica y 0 % de los
DSP**, así que hay espacio de sobra para el filtro propio en hardware; el recurso que hay
que cuidar es la **memoria embebida** (52 % de los M10K), sobre todo si el presupuesto de
latencia (CD-03) pide un buffer compartido más grande.
