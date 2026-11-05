#!/usr/bin/env bash
# Corre toda la verificacion sin tarjeta y resume PASS/FAIL:
#   - pruebas en el PC de sw/common (make -C tests)
#   - testbenches rapidos del SoC: pio, audio, adc (AD7928 y LTC2308), shm
#   - con --all ademas los lentos: vga (~15 min) y codec (~35 min)
# Los testbenches corren de a JOBS en paralelo (por defecto 2).
#
# Uso:  scripts/test-all.sh [--all]
# Requiere scripts/build-soc.sh. Termina con codigo 0 solo si todo pasa.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${JOBS:-2}"
OUT="$HERE/build/sim/test-all"
mkdir -p "$OUT"

TESTS=("pio" "audio" "adc" "adc:-gLTC=1" "shm")
[ "${1:-}" = "--all" ] && TESTS+=("vga" "codec")

declare -A RESULT
if make -C "$HERE/tests" > "$OUT/pc.log" 2>&1; then RESULT[pc]=PASS; else RESULT[pc]=FAIL; fi

run_one() {
    local spec="$1" name="${1%%:*}" opts="" tag
    [[ "$spec" == *:* ]] && opts="${spec#*:}"
    tag="$name${opts:+ ($opts)}"
    if ELAB_OPTS="$opts" "$HERE/scripts/sim-tb.sh" "$name" > "$OUT/${spec//[:=-]/_}.log" 2>&1; then
        echo "$tag|PASS"
    else
        echo "$tag|FAIL"
    fi
}
export -f run_one
export HERE OUT

# Las pruebas de un mismo programa (adc con y sin LTC) comparten build: se compila
# una vez antes y luego se corren en paralelo sin recompilar.
for t in "${TESTS[@]}"; do
    n="${t%%:*}"
    APP="test_$n" "$HERE/scripts/build-niosv.sh" > "$OUT/build_$n.log" 2>&1 || true
done
export NOBUILD=1
printf '%s\n' "${TESTS[@]}" | xargs -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {} > "$OUT/summary.txt"

echo "| Prueba | Resultado |"
echo "|---|---|"
echo "| PC (make -C tests) | ${RESULT[pc]} |"
sort "$OUT/summary.txt" | awk -F'|' '{printf "| tb_%s | %s |\n", $1, $2}'
echo
echo "Logs: $OUT/ y build/sim/tb_<nombre>.log"
! grep -q FAIL "$OUT/summary.txt" && [ "${RESULT[pc]}" = PASS ]
