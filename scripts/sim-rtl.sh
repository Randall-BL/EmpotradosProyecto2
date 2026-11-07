#!/usr/bin/env bash
# Verificacion aislada de un modulo HDL propio en Questa, sin el SoC (HW-16, PR-02):
# compila hw/rtl/*.v y sim/questa/rtl/tb_<nombre>.sv y corre la simulacion en consola.
# Los vectores dorados se leen de sim/vectors/ (scripts/design_fir.py).
#
#   scripts/sim-rtl.sh <nombre>        p. ej.  scripts/sim-rtl.sh filtro_audio
#   scripts/sim-rtl.sh <nombre> gui    abre Questa con el waveform
#
# Termina con codigo 0 solo si el testbench imprime PASS. Transcript en
# build/sim/rtl_<nombre>.log.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:?uso: scripts/sim-rtl.sh <nombre> [gui]}"
TB="$HERE/sim/questa/rtl/tb_$NAME.sv"
[ -f "$TB" ] || { echo "ERROR: no existe $TB" >&2; exit 1; }
source "$HERE/scripts/env.sh" >/dev/null

WORK="$HERE/build/sim/rtl_$NAME.d"
LOG="$HERE/build/sim/rtl_$NAME.log"
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
vlib work >/dev/null
DO="vlog -sv -quiet $HERE/hw/rtl/*.v $TB; vsim -voptargs=+acc tb_$NAME +VEC=$HERE/sim/vectors"
if [ "${2:-}" = "gui" ]; then
    vsim -gui -do "$DO; add wave -r /tb_$NAME/dut/*; run -all"
    exit 0
fi
vsim -c -do "$DO; run -all; quit -f" > "$LOG" 2>&1 || true
grep -E "^# (TB_|OK |Latencia|Comprobaciones|\*\* (Error|Fatal))" "$LOG" | sed 's/^# //'
grep -q "TB_[A-Z0-9_]*: PASS" "$LOG"
