# Versiones fijas del contenedor (DK-02)

Todas las versiones están fijadas en [`Dockerfile`](Dockerfile) (`apt` con
`paquete=versión`, `pip` con `==`, la base por digest). Si una versión de `apt`
desaparece del archivo de Ubuntu (las actualizaciones de seguridad reemplazan a las
anteriores), el build falla en vez de instalar otra cosa: se actualiza aquí y en el
`Dockerfile` en el mismo commit, y se corre `make docker-ci` para confirmar.

Consultadas el 2026-11-03 con `apt-cache policy` dentro de la base.

## Base

| Imagen | Digest |
|---|---|
| `ubuntu:24.04` | `sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852` |

## Paquetes de Ubuntu

| Grupo (capa) | Paquete | Versión | Para qué |
|---|---|---|---|
| Compilación nativa | `gcc` | 4:13.2.0-7ubuntu1 (gcc 13.3) | Pruebas en C de `sw/common` (`make test`), con ASan/UBSan |
| | `libc6-dev` | 2.39-0ubuntu8.9 | |
| | `make` | 4.3-4.1build2 | Makefile raíz (DK-10) |
| | `file` | 1:5.45-3build1 | Comprobar el tipo de ELF (DK-08, `make hps`) |
| | `ca-certificates` | 20260816ubuntu0.24.04.2 | `pip` por HTTPS |
| HDL (CD-13) | `iverilog` | 12.0-2build2 | Simulador de cocotb (`make sim`) |
| | `verilator` | 5.020-1 | `make lint` (`--lint-only -Wall`) |
| ARM del HPS (CD-02) | `gcc-arm-linux-gnueabihf` | 4:13.2.0-7ubuntu1 (gcc 13.3) | U-Boot/SPL, aplicación del HPS (Linux o freestanding) |
| | `libc6-dev-armhf-cross` | 2.39-0ubuntu8cross1 | |
| Imagen de la SD (HP-03) | `fdisk` | 2.39.3-9ubuntu6.6 | `sfdisk` para las particiones |
| | `dosfstools` | 4.2-1.1build1 | `mkfs.fat` |
| | `mtools` | 4.0.43-1build1 | Copiar a la FAT32 sin montar (sin root) |
| | `u-boot-tools` | 2025.10-0ubuntu0.24.04.4 | `mkimage` (scripts de arranque de U-Boot) |
| Python | `python3`, `python3-venv` | 3.12.3-0ubuntu2.1 | Entorno de cocotb |

## Paquetes de Python (venv en `/opt/venv`)

| Paquete | Versión | Para qué |
|---|---|---|
| `cocotb` | 1.9.2 | Testbenches del HDL propio (DK-09, DK-11..13) |
| `numpy` | 2.1.3 | Modelos de referencia y vectores |
| `scipy` | 1.14.1 | `scipy.signal` (diseño y respuesta de los filtros, `scripts/design_fir.py`) |
| `pytest` | 8.3.4 | Runner de los tests de cocotb |

## Lo que se dejó fuera y por qué

| Herramienta | Motivo |
|---|---|
| `gcc-arm-none-eabi` + `libnewlib-arm-none-eabi` | Con ella la imagen pasaba de 2 GB (las multilib de newlib ocupan ~790 MB). `arm-linux-gnueabihf-gcc` compila U-Boot y, con `-ffreestanding -nostdlib`, una aplicación bare metal (DK-07). Si CD-02 elige bare metal con newlib, se vuelve a evaluar. |
| `cmake`, `git` | No los usa ningún objetivo del contenedor (el `git` es del anfitrión; el CMake del Nios V es nativo). |
| Quartus, Platform Designer, Questa, toolchain del Nios V | Ver "Qué se hace fuera" en [README.md](README.md). |
