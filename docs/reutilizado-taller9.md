# Material reutilizado del Taller 9

El Taller 9 (SoC con Nios V/m en Platform Designer, verificado por simulación en
Questa) es el punto de partida que recomienda el enunciado (§2, p. 2). Este documento
registra qué se tomó de él, dónde quedó y qué se cambió, como evidencia para el rubro
"Herramientas de ingeniería" (§5, p. 8).

- **Origen:** `~/Escritorio/Tec/SistemasEmpotrados/Taller9/Taller09_NiosV_SoC_src/`
  (en la tabla, las rutas de origen son relativas a esa carpeta).
- **Modo:** se **copió**; el Taller 9 original no se movió ni se modificó.
- **Fecha:** 2026-10-29.

## Archivos copiados

| Origen (Taller 9) | Destino (proyecto) | Para qué se usa | Cambios |
|---|---|---|---|
| `hw/build_system.tcl` | `hw/soc/build_system.tcl` | Descripción en Tcl del SoC base: Nios V/m, RAM on-chip de 256 KB, JTAG UART, `pio_out` (8 bits) y `pio_in` (4 bits), mapa de direcciones e IRQ de la JTAG UART. Dispositivo `5CSEMA5F31C6` (DE1-SoC). Sobre él se agregan los periféricos del reproductor (TODO.md, HW-*). | Encabezado con el origen y cómo regenerar. Sistema renombrado `hello` → `audio_soc` (`create_system` y `save_system`). Comentario de la RAM corregido (`hello.hex` → `ram.hex`, que es el archivo real). Instancias, parámetros y direcciones sin cambios. |
| `scripts/env.sh` | `scripts/env.sh` | Exporta `PATH`, `QUARTUS_ROOTDIR`, `QSYS_ROOTDIR` y `SALT_LICENSE_SERVER` para Quartus, Platform Designer, Nios V y Questa. Lo cargan los demás scripts. | Encabezado y mensaje final hablan del proyecto en vez del Taller 09. Variables y valores por defecto sin cambios. |
| `scripts/build-hw.sh` | `scripts/build-soc.sh` | Genera el `.qsys` desde el Tcl, el RTL de síntesis, el `.sopcinfo` (para el BSP) y el testbench Verilog para Questa. | Renombrado a `build-soc.sh` (en el proyecto habrá más "hardware" que el SoC). Rutas `hw/` → `hw/soc/`, sistema `hello` → `audio_soc` (variable `SYS`). Ahora corre `qsys-script` si falta el `.qsys`, si el Tcl es más nuevo o si `REGEN_QSYS=1`, y avisa si el `.qsys` es más nuevo (editado en la GUI); en el original esa línea estaba comentada. |
| `scripts/build-sw.sh` | `scripts/build-niosv.sh` | Genera el BSP HAL con `niosv-bsp`, la aplicación CMake con `niosv-app`, compila el ELF y copia `ram.hex` al directorio de simulación. | Renombrado a `build-niosv.sh` (habrá también software del HPS). Fuentes `sw/src` → `sw/niosv/src`. Salida `build/` → `build/niosv/`, y solo se borra esa carpeta (antes borraba todo `build/`, que ahora comparte con el HPS). ELF `hello.elf` → `audio_player.elf`. Rutas del `.sopcinfo` y del testbench ajustadas a `hw/soc/`. |
| `scripts/sim.sh` | `scripts/sim-soc.sh` | Simulación RTL del SoC completo en Questa (consola o GUI), con `ld_debug` para poder forzar y graficar puertos. | Renombrado a `sim-soc.sh` para distinguirlo de `make sim` (cocotb en el contenedor). Testbench en `hw/soc/gen/testbench/mentor`. Los `.do` se leen de `sim/questa/`. |
| `scripts/pio.do` | `sim/questa/pio.do` | Estímulo de Questa: fuerza la entrada `pio_in` y registra todas las señales. Plantilla para forzar KEY/SW en simulaciones del SoC. | Ruta jerárquica `/hello_tb/hello_inst/` → `/audio_soc_tb/audio_soc_inst/`. Encabezado. |
| `scripts/wave.do` | `sim/questa/wave.do` | Señales de los PIO en el waveform. Plantilla para los waveforms de evidencia. | Misma ruta jerárquica actualizada. Encabezado. |
| `scripts/evidence.do` | `sim/questa/evidence.do` | Acomoda las ventanas de Questa para capturar waveform y transcript en una sola imagen (evidencia para el README). | Solo el encabezado; avisa que el zoom (2.55–2.85 ms) es del programa de prueba y hay que ajustarlo. |
| `sw/src/hello_world.c` | `sw/niosv/src/main.c` | Prueba de humo: "hola mundo" por JTAG UART y escritura/lectura de PIO con punteros `volatile` a las bases de `system.h`, sin `alt_*` ni `IOWR`/`IORD`. Es el patrón de acceso a periféricos que exige el enunciado (§3, p. 5). | Comentario de encabezado reescrito para el proyecto. Código sin cambios. Se reemplazará por el lazo principal del reproductor (NV-*). |
| `README.md` (secciones "Requisitos" y "Configuración del entorno") | `docs/entorno-nativo.md` | Instalación y configuración del entorno nativo; base para la sección de entorno del README del proyecto (§5, p. 8). | Solo se tomaron esas secciones. Se agregó la tabla de variables, la razón para que Quartus quede fuera de Docker y el flujo con los scripts renombrados. Se quitó lo propio del taller (estructura, trabajo evaluado). |

Cambio asociado fuera de lo copiado: `.gitignore` ignora `hw/soc/*.qsys`, `hw/soc/gen/`,
`hw/soc/*_tb.csv` y `hw/soc/*_tb.spd`, que genera `build-soc.sh`. El `.sopcinfo` y
`build/` ya estaban ignorados.

## No copiado

| Origen (Taller 9) | Motivo |
|---|---|
| `hw/hello.qsys` | Se regenera con `qsys-script` desde `build_system.tcl`; `build-soc.sh` lo hace solo. El grupo decidió no versionarlo: la única fuente del SoC es el Tcl (tarea HW-01 del TODO). |
| `hw/hello.sopcinfo`, `hw/hello_tb.csv`, `hw/hello_tb.spd` | Los genera `qsys-generate`. |
| `hw/gen/` (≈100 MB: síntesis, testbench, `simulation/`) | Lo genera `qsys-generate`. |
| `build/` (BSP compilado, `hello.elf`, `ram.hex`, CMake) | Lo generan `niosv-bsp`, `niosv-app` y `make`. |
| `README.txt` | Datos de entrega del taller (nombre, carné, mapa de los PIO del taller). |
| `Taller_09_NiosV_SoC.pdf`, informe, `.tar.gz`, `evidencia_taller09.png` | Enunciado y entregables del taller, no del proyecto. |

## Cómo comprobar que lo reutilizado funciona

```bash
scripts/build-soc.sh && scripts/build-niosv.sh && scripts/sim-soc.sh
```

El transcript de Questa debe mostrar lo mismo que en el Taller 9: cinco líneas
"Hola mundo, aqui el Nios V/m reportandose N...", luego `PIO_OUT <= 0x01`,
`PIO_OUT <= 0xa5`, `PIO_OUT <= 0xff`, `PIO_IN  = 0x0000000a` y "Fin del programa.".

## Qué falta adaptar (queda en el TODO.md)

- PIO definitivos: `pio_in` → KEY[3:0] con interrupción por flanco, `pio_out` → LEDR,
  y un PIO nuevo para SW (HW-02).
- Agregar HPS, audio, ADC, video, 7 segmentos y módulo propio al Tcl (HW-05 a HW-17).
- Proyecto Quartus con top level, pines y `.sdc`: el Taller 9 solo simulaba y no
  tenía (HW-19 a HW-22).
- Ajustes del BSP para memoria y las fuentes de `sw/common/` en la aplicación (NV-01, NV-02).
