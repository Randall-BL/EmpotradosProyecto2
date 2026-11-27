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
| `0x0004_0100` | `0x0004_011F` | 32 B | `filtro` | `filtro_audio` (propio) | Filtro FIR estéreo + ganancia ([modulo-filtro.md](modulo-filtro.md)) |
| `0x0004_0120` | `0x0004_012F` | 16 B | `seg7` | `seg7_time` (propio) | Tiempo MM:SS y canción en HEX5..HEX0 |
| `0x0004_2000` | `0x0004_3FFF` | 8 KB | `char_buf` (caracteres) | Character Buffer | Texto 80 × 60: `base + (y << 7) + x` *(HW-09)* |
| `0x0005_0000` | `0x0005_FFFF` | 64 KB | `cpu.dm_agent` | Módulo de depuración del Nios V | JTAG debug |

Las filas marcadas con la tarea entre paréntesis se agregan al Tcl en esa tarea.

## Canal HPS ↔ Nios V (HW-06)

| Vista | Dirección de `shm` | Puerto | Estado |
|---|---|---|---|
| Nios V | `0x0002_0000` – `0x0002_FFFF` (`SHM_BASE`) | `shm.s1` | En el SoC; verificado con `tb_shm` (bus de datos, bus de direcciones, March C-, accesos de 8/16 bits) |
| HPS | `0xFF20_0000` – `0xFF20_FFFF` (offset 0 del puente liviano H2F; `LWH2F_BASE` en `sw/hps/src/devmem.h`) | `shm.s2` | En el SoC (`map hps.h2f_lw_axi_master shm.s2 0x0`); probado en la tarjeta: el HPS publica la tabla de pistas y llena el ring |

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

El mapa exportado de Platform Designer (`python3 scripts/report-memory-map.py`) está en
[mapa-memoria-generado.md](mapa-memoria-generado.md) y coincide con esta tabla. Incluye
también la vista desde el HPS (HW-24), resumida en la sección siguiente.

## Mapa visto desde el HPS (HW-24)

Direcciones físicas del ARM que usa el software del HPS. La aplicación (`hps_player`)
solo toca las tres marcadas, por `mmap` de `/dev/mem` con punteros `volatile`
(`sw/hps/src/devmem.c`); el resto lo manejan los drivers del kernel o U-Boot.

| Base | Fin | Tamaño | Región | Quién la usa |
|---|---|---|---|---|
| `0x0000_0000` | `0x3FFF_FFFF` | 1 GB | DDR3 del HPS | Linux; el buffer de lectura anticipada (4 MiB) de `prefetch.c` |
| `0xFF20_0000` | `0xFF20_FFFF` | 64 KB | Puente liviano H2F → `shm.s2` | **`hps_player`**: control, tabla de pistas y ring (`shm_proto.h`) |
| `0xFF70_4000` | `0xFF70_4FFF` | 4 KB | Controlador SD/MMC | Driver `dw_mmc` del kernel (con el parche de tiempo de espera) |
| `0xFF70_6000` | `0xFF70_6FFF` | 4 KB | FPGA Manager | **`hps_player`**: `stat[2:0]` = 4 indica que la FPGA está configurada antes de tocar el puente; U-Boot carga el `.rbf` |
| `0xFF80_0000` | `0xFF80_0FFF` | 4 KB | Registros del L3 (`remap`) | U-Boot (`bridge enable`); escribirlo con Linux corriendo congela el sistema (CD-02) |
| `0xFFC0_2000` | `0xFFC0_20FF` | 256 B | UART0 | Consola del kernel a 115 200 baudios |
| `0xFFD0_5000` | `0xFFD0_50FF` | 256 B | Reset Manager | **`hps_player`**: saca de reset el puente liviano (`brgmodrst` bit 1) |

El puente liviano ocupa 2 MB (`0xFF20_0000` – `0xFF3F_FFFF`); solo `shm` está conectada,
al inicio. El Nios V ve la misma memoria en `0x0002_0000`.

## Uso de la RAM del Nios V por el programa (NV-16)

Medido con `scripts/report-memory.sh <elf>` (`riscv32-unknown-elf-size`), BSP de
`sw/niosv/bsp.tcl` (picolibc, sin `alt_load`, drivers HAL reducidos y sin drivers de
los IP):

| Programa | `.text` + `.rodata` + `.entry`/`.exceptions` | `.rwdata` | `.bss` | Total | % de 128 KB |
|---|---|---|---|---|---|
| `test_adc` | 7 044 | 516 | 312 | 7 872 | 6 % |
| `test_pio` | 8 308 | 516 | 320 | 9 144 | 7 % |
| `test_audio` | 8 444 | 516 | 316 | 9 276 | 7 % |
| `test_tono` (drivers + pantalla + volumen + tono) | 19 004 | 516 | 328 | 19 848 | 15 % |
| prueba de humo con `printf` (`sw/niosv/src`, versión inicial) | 20 684 | 516 | 312 | 21 512 | 16 % |
| **`audio_player`** (aplicación final, `sw/niosv/src`) | 20 560 | 500 | 10 828 | **31 888** | **24 %** |

- El `printf` de picolibc con soporte de punto flotante cuesta ~10 KB (`vfprintf`,
  `__dtoa_engine`, `__udivdi3`): la aplicación final no lo usa en el lazo de audio y, si
  hace falta para mediciones, conviene formatear enteros a mano como `sw/common/src/screen.c`.
- La tabla de seno de `tone.c` (2 KB) y la de volumen (64 B) van en `.rodata`.
- En la aplicación final, 9 600 B del `.bss` son las dos líneas de retardo de la
  reverberación (2 × 2 400 muestras). No hay memoria dinámica: todo el estado es estático.
- Quedan 99 184 B (76 %) para pila y heap. Los buffers de audio grandes no van aquí sino
  en `shm` (ring de 32 KB) y, del lado del HPS, en la DDR3 (4 MiB de lectura anticipada).
