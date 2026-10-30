# Entorno nativo: Quartus, Platform Designer, Nios V y Questa

> Origen: sección "Requisitos" y "Configuración del entorno" del README del Taller 9
> (ver [reutilizado-taller9.md](reutilizado-taller9.md)). Se adaptaron las rutas y los
> nombres de los scripts a la estructura del proyecto.

Estas herramientas se instalan y ejecutan **de forma nativa**, fuera del contenedor
Docker: necesitan acceso al USB-Blaster (JTAG), tienen interfaz gráfica, ocupan
decenas de GB y Questa usa una licencia node-locked a la MAC del equipo.

## Requisitos

- **Quartus Prime Lite 25.1std** (soporta Cyclone V / DE1-SoC y Nios V).
- Componente **Nios V / RiscFree**: trae el toolchain `riscv32-unknown-elf` y las
  utilidades `niosv-*`.
- **Questa Altera Starter FPGA Edition** (simulador, incluido con Quartus).
- Licencia de Questa (Starter es gratuita, node-locked). **La variable es
  `SALT_LICENSE_SERVER`**, no `LM_LICENSE_FILE`.

> No hay un simulador de instrucciones ("ISS") como en Nios II: la ejecución sin
> tarjeta se hace por **simulación RTL** del SoC completo en Questa.

## Configuración

`scripts/env.sh` deja listas todas las variables (`source scripts/env.sh`). Los demás
scripts lo cargan solos. Por defecto usa:

| Variable | Valor por defecto | Cómo cambiarlo |
|---|---|---|
| `ALTERA_ROOT` | `/opt/altera_lite/25.1std` | `export ALTERA_ROOT=/ruta/a/altera_lite/25.1std` antes de llamar a los scripts |
| `SALT_LICENSE_SERVER` | `~/altera_lite/LR-203954_License.dat` | `export SALT_LICENSE_SERVER=/ruta/a/su/licencia.dat` |

Cada integrante exporta sus propias rutas; los valores por defecto son los de la
máquina donde se armó el Taller 9.

### Rutas que agrega `env.sh` al `PATH`

| Ruta | Herramientas |
|---|---|
| `quartus/bin` | `quartus_sh`, `quartus_pgm` |
| `quartus/sopc_builder/bin` | `qsys-generate`, `qsys-script`, `qsys-edit`, `ip-catalog` |
| `niosv/bin` | `niosv-bsp`, `niosv-app`, `niosv-download`, `elf2hex` |
| `riscfree/toolchain/riscv32-unknown-elf/bin` | compilador RISC-V (`riscv32-unknown-elf-gcc`) |
| `questa_fse/bin` | `vsim` (Questa) |

También fija `QSYS_ROOTDIR`, para no tomar por error una instalación vieja de Quartus.

### Licencia de Questa (SALT, no FLEXlm)

Si se usa `LM_LICENSE_FILE` (como en versiones viejas de ModelSim) aparece el error
`Invalid license environment. Application closing.`: es el síntoma de no haber usado
`SALT_LICENSE_SERVER`.

La licencia Starter está atada a la MAC del equipo. Para confirmar que corresponde:

```bash
cat /sys/class/net/*/address                          # MACs del equipo
grep -o 'HOSTID=[0-9a-f]*' "$SALT_LICENSE_SERVER"     # debe coincidir con una
```

### Verificar que todo responde

```bash
source scripts/env.sh
riscv32-unknown-elf-gcc --version   # toolchain RISC-V
qsys-generate --help                # Platform Designer
niosv-bsp --help                    # utilidades Nios V
vsim -version                       # Questa
```

## Flujo del SoC (heredado del Taller 9)

```bash
scripts/build-soc.sh       # .qsys desde hw/soc/build_system.tcl + RTL + .sopcinfo + testbench
scripts/build-niosv.sh     # BSP + app de sw/niosv/src + audio_player.elf + ram.hex
scripts/sim-soc.sh         # Questa en consola: transcript con la salida de la JTAG UART
scripts/sim-soc.sh gui     # Questa con GUI y waveform de los PIO (captura de evidencia)
```

`REGEN_QSYS=1 scripts/build-soc.sh` fuerza regenerar el `.qsys` desde el Tcl. La
salida generada (`hw/soc/gen/`, `build/`, `*.sopcinfo`) no se versiona.

Notas aprendidas en el Taller 9:

- `sim-soc.sh` elabora con `ld_debug` (`-voptargs=+acc`); sin eso Questa optimiza los
  puertos de los PIO y no se pueden forzar ni graficar.
- La JTAG UART simulada es lenta: hacen falta ~6 ms de simulación (`SIM_TIME`) para
  ver toda la salida del programa de prueba.
