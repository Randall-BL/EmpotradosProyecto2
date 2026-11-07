#!/usr/bin/env bash
# Copia los resultados de la ultima corrida de cada testbench a docs/verificacion/,
# como evidencia versionada: de cada transcript de Questa (build/sim/tb_*.log) toma
# solo la salida del testbench (desde "TB_<NOMBRE>: inicio" hasta PASS/FAIL), y
# agrega la salida de las pruebas en el PC (make -C tests).
#
# Uso:  scripts/test-all.sh --all && scripts/export-results.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST="$HERE/docs/verificacion/resultados"
mkdir -p "$DST"
COMMIT="$(git -C "$HERE" rev-parse --short HEAD)"

for log in "$HERE"/build/sim/tb_*.log "$HERE"/build/sim/rtl_*.log; do
    [ -f "$log" ] || continue
    name="$(basename "$log" .log)"
    tb="${name%%__*}"
    opts="${name#"$tb"}"
    out="$DST/$name.txt"
    {
        echo "# $tb${opts:+ (elaboracion: ${opts//_/ })}"
        echo "# Transcript: build/sim/$name.log, $(date -r "$log" '+%Y-%m-%d %H:%M'), commit $COMMIT"
        echo
        sed -n '/TB_[A-Z0-9_]*: inicio/,/TB_[A-Z0-9_]*: \(PASS\|FAIL\)/p' "$log" | sed 's/^# //'
    } > "$out"
    grep -q 'TB_[A-Z0-9_]*: PASS' "$out" && r=PASS || r=FAIL
    echo "$name: $r"
done

{
    echo "# Pruebas en el PC (make -C tests)"
    echo "# $(date '+%Y-%m-%d %H:%M'), commit $COMMIT"
    echo
    make -s -C "$HERE/tests" 2>&1 | grep -v -E '^make|directorio'
} > "$DST/pc_tests.txt"
echo "pc_tests: $(grep -c PASS "$DST/pc_tests.txt") PASS"
