# Mapa de memoria del SoC (visto desde el Nios V)

Fuente: `hw/soc/build_system.tcl` (única fuente del SoC). Los nombres de las bases son los
que genera el BSP en `system.h` (`<INSTANCIA>_BASE`). Decisiones: [CD-06](../decisiones/CD-06.md)
(memoria), [CD-07](../decisiones/CD-07.md) (audio), [CD-09](../decisiones/CD-09.md) (video).

| Base | Fin | Tamaño | Instancia | Componente | Uso |
|---|---|---|---|---|---|
| `0x0000_0000` | `0x0001_FFFF` | 128 KB | `ram` | On-Chip Memory | Código, datos, pila (instrucciones y datos del Nios V) |
| `0x0002_0000` | `0x0002_FFFF` | 64 KB | `shm` | On-Chip Memory (doble puerto) | Buffer compartido HPS ↔ Nios V (`s1`; `s2` para el HPS) |
| `0x0004_0000` | `0x0004_0007` | 8 B | `juart` | JTAG UART | stdout de depuración y mediciones |
| `0x0004_0010` | `0x0004_001F` | 16 B | `pio_led` | PIO (salida, 10 bits) | LEDR[9:0] |
| `0x0004_0020` | `0x0004_002F` | 16 B | `pio_key` | PIO (entrada, 4 bits, IRQ) | KEY[3:0], flanco de bajada |
| `0x0004_0030` | `0x0004_003F` | 16 B | `pio_sw` | PIO (entrada, 10 bits, IRQ) | SW[9:0], cualquier flanco |
| `0x0004_0040` | `0x0004_007F` | 64 B | `cpu.timer_sw_agent` | Timer del Nios V (`mtime`) | Tiempo y mediciones |
| `0x0004_0080` | `0x0004_008F` | 16 B | `audio` | Audio (University Program) | FIFO de salida al WM8731 *(HW-07)* |
| `0x0004_0090` | `0x0004_009F` | 16 B | `av_config` | Audio and Video Config | I²C de configuración del códec *(HW-07)* |
| `0x0004_00A0` | `0x0004_00BF` | 32 B | `adc` | DE1-SoC ADC Controller | Potenciómetro de volumen *(HW-08)* |
| `0x0004_00C0` | `0x0004_00C7` | 8 B | `char_buf` (control) | Character Buffer | Limpiar pantalla, resolución *(HW-09)* |
| `0x0004_0100` | `0x0004_01FF` | 256 B | — | reservado para B | Módulo de filtro y `seg7_time` *(HW-10, HW-17)* |
| `0x0004_2000` | `0x0004_3FFF` | 8 KB | `char_buf` (caracteres) | Character Buffer | Texto 80 × 60: `base + (y << 7) + x` *(HW-09)* |
| `0x0005_0000` | `0x0005_FFFF` | 64 KB | `cpu.dm_agent` | Módulo de depuración del Nios V | JTAG debug |

Las filas marcadas con la tarea entre paréntesis se agregan al Tcl en esa tarea.

## Canal HPS ↔ Nios V (HW-06)

| Vista | Dirección de `shm` | Puerto | Estado |
|---|---|---|---|
| Nios V | `0x0002_0000` – `0x0002_FFFF` (`SHM_BASE`) | `shm.s1` | En el SoC; verificado con `tb_shm` (bus de datos, bus de direcciones, March C-, accesos de 8/16 bits) |
| HPS | Propuesta: `0xFF20_0000` (offset 0 del bridge liviano H2F) | `shm.s2` | Lo conecta D en HW-05 con `add_connection hps.h2f_lw_axi_master shm.s2` |

Los dos puertos son independientes (RAM de doble puerto): el HPS escribe muestras y el
Nios V las lee sin arbitraje de bus. El handshake son los índices de productor y
consumidor del ring buffer al inicio de `shm` (protocolo SC-13); cada índice lo escribe
un solo lado, así que no hace falta exclusión mutua ni buzón (CD-06).

## Interrupciones (`platform_irq_rx` del Nios V)

| IRQ | Fuente | Uso |
|---|---|---|
| 1 | `juart` | JTAG UART (HAL de stdout) |
| 2 | `pio_key` | Botones play/pausa, stop, anterior, siguiente |
| 3 | `pio_sw` | Cambio de switches (si CD-08 elige interrupción) |
| 4 | `audio` | FIFO de salida con ≥ 75 % libre (relleno por interrupción, NV-06) |

## Uso de la memoria embebida (M10K)

| Bloque | Bytes | Nota |
|---|---|---|
| `ram` | 131 072 | CD-06 |
| `shm` | 65 536 | CD-06 |
| `char_buf` | ≈ 8 192 | 80 × 60 caracteres (HW-09) |
| FIFO del IP de Audio | ≈ 1 024 | 128 palabras × 2 canales (HW-07) |
| Total previsto | ≈ 206 KB | ≈ 41 % de los bits de M10K del 5CSEMA5 |

Medido en la compilación (HW-21, `scripts/report-resources.py`): 1 648 768 bits (41 %) en
**207 de 397 bloques M10K (52 %)**. La diferencia es porque cada memoria ocupa bloques
enteros de 10 Kbit; por ejemplo, el Character Buffer reserva 8 KB (direcciones de 13 bits)
para 4 800 caracteres.

El mapa final, exportado de Platform Designer y visto también desde el HPS, se cierra en
HW-24.
