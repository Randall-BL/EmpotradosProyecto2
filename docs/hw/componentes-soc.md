# Justificación de los componentes del SoC (HW-23)

El enunciado pide que todo componente tenga una función justificada (§3-HW p.5). Esta
tabla relaciona cada instancia de `hw/soc/build_system.tcl` con su función, el requisito
que cubre y lo que cuesta. Recursos medidos en la compilación de Quartus
(`python3 scripts/report-resources.py`); se actualizan en cada entrega.

| Instancia | Componente | Función | Requisito | ALMs | M10K |
|---|---|---|---|---|---|
| `cpu` | Nios V/m (pipeline) | Procesador: drivers, interrupciones, máquina de reproducción, UI | §2, §3-SW | 1860 | 2 |
| `ram` | On-Chip Memory 128 KB | Código y datos del Nios V | §3-SW (gestión de memoria), CD-06 | 38 | 128 |
| `shm` | On-Chip Memory 64 KB, doble puerto | Buffer compartido con el HPS (SD y metadatos) | §3-SW (SD mediante el HPS), CD-06 | 2 | 64 |
| `pio_key` | PIO entrada 4 bits, IRQ por flanco | Botones play/pausa, stop, anterior, siguiente | §3-F (botones por interrupción) | 11 | 0 |
| `pio_sw` | PIO entrada 10 bits, IRQ | Selección de filtro | §3-SIS (filtros por switches) | 28 | 0 |
| `pio_led` | PIO salida 10 bits | Estado de reproducción y filtro activo | §3-F (estado e indicador de filtro) | 6 | 0 |
| `audio_pll` | Audio PLL 12.288 MHz | MCLK del códec (256 × 48 kHz) | §3-HW (audio en la FPGA), CD-07 | 0 | 0 |
| `audio` | Audio (University Program), solo salida | FIFO y serializador hacia el DAC del WM8731 | §3-HW (IP de Audio) | 79 | 2 |
| `av_config` | Audio and Video Config | Configuración I²C del códec | §3-HW (IP de Audio), CD-07 | 111 | 0 |
| `adc` | DE1-SoC ADC Controller, 2 canales | Lectura del potenciómetro de volumen (2 canales para que CH0 funcione con AD7928 y con LTC2308) | §3-SIS (volumen por ADC) | 127 | 0 |
| `video_pll` | Video PLL 25 MHz | Reloj de píxel VGA 640 × 480 | §3-F (metadatos en pantalla), CD-09 | 0 | 0 |
| `char_buf` | Character Buffer 80 × 60 | Texto de la pantalla | §3-F, CD-09 | 52 | 8 |
| `video_fifo` | Dual-Clock FIFO | Cruce de 50 MHz a 25 MHz del video | CD-09 | 38 | 1 |
| `vga` | VGA Controller | Sincronismos y salida al DAC de video | CD-09 | 35 | 0 |
| `juart` | JTAG UART | stdout de depuración y **mediciones de tiempos** (PR-07) | §3-F (rendimiento, medir) | 60 | 2 |
| `cpu.timer_sw_agent` | Timer del Nios V (`mtime`) | Base de tiempo para mediciones y antirrebote | §3-F (rendimiento) | dentro de `cpu` | — |
| `cpu.dm_agent` | Módulo de depuración | Carga del programa y depuración por JTAG (`niosv-download`) | Desarrollo | dentro de `cpu` | — |
| *(B)* filtro propio | Módulo personalizado con filtro DSP en HDL | Filtro obligatorio en hardware | §3-HW, §3-SIS | *HW-17* | |
| *(B)* `seg7_time` | Displays de 7 segmentos | Tiempo MM:SS | §3-HW (7 segmentos) | *HW-10* | |
| *(D)* HPS | Cyclone V HPS | SD, FAT32 y parseo de WAV | §3-SW | *HW-05* | |

Componentes que agrega Platform Designer solos: `mm_interconnect_0` (buses Avalon,
762 ALMs), `rst_controller*` (sincronización de reset) e `instrumentation_fabric` (JTAG).

## Componentes evaluados y descartados

| Componente | Por qué no está |
|---|---|
| SDRAM de 64 MB + controlador + PLL | La RAM on-chip alcanza (CD-06); evita un componente sin función |
| RGB Resampler en la cadena de video | El VGA de la DE1-SoC usa 10 bits por color, igual que el Character Buffer (HW-09) |
| Entrada de audio (grabación) | El reproductor no graba: el IP de Audio es solo de salida |
| Pixel Buffer DMA | Solo se muestra texto; un framebuffer de 640 × 480 costaría ~600 KB |
| Buzón (mailbox) HPS ↔ Nios V | Los índices del ring buffer en `shm` ya son el handshake (CD-06) |

## JTAG UART en la versión final

Se mantiene porque `PR-07` mide por ella los tiempos de arranque, respuesta a botones y
cambio de canción. Si en la entrega final no se usa, se quita con borrar su bloque en el
Tcl (`add_instance juart ...`, su conexión y su IRQ) y poner `hal.stdout none` en
`sw/niosv/bsp.tcl`; ahorra 60 ALMs y 2 M10K.
