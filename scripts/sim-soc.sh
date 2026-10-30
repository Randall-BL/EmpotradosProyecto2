#!/usr/bin/env bash
# Corre la simulacion RTL del SoC completo en Questa (modo consola) y muestra
# el transcript, donde aparece la salida del printf del Nios V por la JTAG UART.
#
# Origen: Taller 9, scripts/sim.sh (ver docs/reutilizado-taller9.md).
# Es la simulacion nativa del SoC (Nios V + IP de Altera); las pruebas de los
# modulos HDL propios con cocotb corren aparte, en el contenedor (make sim).
#
# Requiere haber corrido build-soc.sh y build-niosv.sh
#
# Uso:
#   scripts/sim-soc.sh            # modo consola (headless), imprime el transcript
#   scripts/sim-soc.sh gui        # abre Questa con GUI (para capturar el waveform)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"

MENTOR="$HERE/hw/soc/gen/testbench/mentor"
DO_DIR="$HERE/sim/questa"
if [ ! -f "$MENTOR/msim_setup.tcl" ]; then
    echo "ERROR: falta el testbench. Corra primero scripts/build-soc.sh" >&2
    exit 1
fi
cd "$MENTOR"
rm -rf work ./*_lib transcript 2>/dev/null || true

# ld_debug elabora con -voptargs=+acc: sin eso los puertos de los PIO quedan
# optimizados y no se pueden forzar ni ver en el waveform.
# pio.do fuerza pio_in = 4'hA y registra todas las senales.
# 6 ms: la JTAG UART simulada es lenta y el printf de PIO_IN sale despues de 3 ms.
SIM_TIME="${SIM_TIME:-6 ms}"

if [ "${1:-}" = "gui" ]; then
    # GUI: compila, elabora, fuerza pio_in, agrega las senales de los PIO al
    # waveform, corre la simulacion y acomoda las ventanas para la captura.
    vsim -gui -do "source msim_setup.tcl; file_copy; ld_debug; do $DO_DIR/pio.do; do $DO_DIR/wave.do; run $SIM_TIME; do $DO_DIR/evidence.do"
else
    # Consola: compila, elabora, fuerza pio_in, corre y sale.
    vsim -c -do "source msim_setup.tcl; file_copy; ld_debug; do $DO_DIR/pio.do; run $SIM_TIME; quit -f"
fi
