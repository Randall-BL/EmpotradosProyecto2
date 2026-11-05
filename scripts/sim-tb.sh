#!/usr/bin/env bash
# Corre un testbench autoverificable del SoC en Questa (modo consola).
#
#   scripts/sim-tb.sh <nombre>        p. ej.  scripts/sim-tb.sh pio
#
# Compila el programa de prueba sw/niosv/tests/test_<nombre> (APP de
# build-niosv.sh), lo carga en la RAM simulada y ejecuta sim/questa/tb_<nombre>.sv,
# que instancia el SoC y verifica sus salidas. Termina con codigo 0 solo si el
# testbench imprime "PASS". El transcript queda en build/sim/tb_<nombre>.log.
#
# Requiere haber corrido scripts/build-soc.sh (testbench generado del SoC).
# Con NOBUILD=1 no recompila el software (reusa el ram.hex actual).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:?uso: scripts/sim-tb.sh <nombre>  (p. ej. pio)}"
TB="$HERE/sim/questa/tb_$NAME.sv"
APP_DIR="$HERE/sw/niosv/tests/test_$NAME"
[ -f "$TB" ] || { echo "ERROR: no existe $TB" >&2; exit 1; }

source "$HERE/scripts/env.sh" >/dev/null

if [ "${NOBUILD:-0}" != "1" ] && [ -d "$APP_DIR" ]; then
    APP="test_$NAME" "$HERE/scripts/build-niosv.sh" > "$HERE/build/niosv-test_$NAME.log" 2>&1 \
        || { echo "ERROR: fallo build-niosv.sh, ver build/niosv-test_$NAME.log" >&2; exit 1; }
fi

MENTOR="$HERE/hw/soc/gen/testbench/mentor"
[ -f "$MENTOR/msim_setup.tcl" ] || { echo "ERROR: corra primero scripts/build-soc.sh" >&2; exit 1; }
mkdir -p "$HERE/build/sim"
LOG="$HERE/build/sim/tb_$NAME.log"

cd "$MENTOR"
rm -rf work ./*_lib transcript 2>/dev/null || true
# TOP_LEVEL_NAME antes de msim_setup.tcl: se elabora nuestro testbench en vez del
# audio_soc_tb generado; audio_soc se resuelve en la libreria audio_soc_inst.
vsim -c -do "set TOP_LEVEL_NAME tb_$NAME; source msim_setup.tcl; file_copy; dev_com; com; \
vlog -sv -work work $TB; elab; run -all; quit -f" > "$LOG" 2>&1 || true

grep -E "^# (TB_|\[|Respuesta|\*\* (Error|Fatal))|OK |FALLO" "$LOG" | sed 's/^# //'
if grep -q "TB_[A-Z_]*: PASS" "$LOG"; then
    exit 0
fi
echo "Ver transcript completo: $LOG" >&2
exit 1
