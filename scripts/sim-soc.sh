#!/usr/bin/env bash
# Corre la simulacion RTL del SoC completo en Questa (modo consola) y muestra
# el transcript, donde aparece la salida del printf del Nios V por la JTAG UART.
#
# Origen: Taller 9, scripts/sim.sh (ver docs/reutilizado-taller9.md).
# Es la simulacion nativa del SoC (Nios V + IP de Altera); las pruebas de los
# modulos HDL propios con cocotb corren aparte, en el contenedor (make sim).
#
# Requiere haber corrido build-soc.sh (compila el software por su cuenta)
#
# Uso:
#   scripts/sim-soc.sh            # modo consola (headless), imprime el transcript
#   scripts/sim-soc.sh gui        # abre Questa con GUI (para capturar el waveform)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"

# Compila la prueba de humo (sw/niosv/src): sim-tb.sh deja en la RAM simulada el
# programa de su testbench. NOBUILD=1 reusa el ram.hex que ya este copiado.
if [ "${NOBUILD:-0}" != "1" ]; then
    "$HERE/scripts/build-niosv.sh" > "$HERE/build/niosv-smoke.log" 2>&1 \
        || { echo "ERROR: fallo build-niosv.sh, ver build/niosv-smoke.log" >&2; exit 1; }
fi

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
# El top es sim/questa/tb_smoke.sv (no el audio_soc_tb generado, ver su encabezado);
# tb_smoke pone SW = 10'h00A. pio.do registra todas las senales.
# 4 ms: la JTAG UART simulada es lenta; el ultimo printf sale cerca de 3.5 ms.
SIM_TIME="${SIM_TIME:-4 ms}"

if [ "${1:-}" = "gui" ]; then
    # GUI: compila, elabora, fuerza pio_in, agrega las senales de los PIO al
    # waveform, corre la simulacion y acomoda las ventanas para la captura.
    vsim -gui -do "set TOP_LEVEL_NAME tb_smoke; source msim_setup.tcl; file_copy; dev_com; com; vlog -sv -work work $HERE/sim/questa/tb_smoke.sv; elab_debug; do $DO_DIR/pio.do; do $DO_DIR/wave.do; run $SIM_TIME; do $DO_DIR/evidence.do"
else
    # Consola: compila, elabora, fuerza pio_in, corre y sale.
    vsim -c -do "set TOP_LEVEL_NAME tb_smoke; source msim_setup.tcl; file_copy; dev_com; com; vlog -sv -work work $HERE/sim/questa/tb_smoke.sv; elab_debug; do $DO_DIR/pio.do; run $SIM_TIME; quit -f"
fi
