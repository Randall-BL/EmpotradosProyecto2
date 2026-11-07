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
# Varios testbenches pueden correr a la vez (cada uno en build/sim/tb_<nombre>.d).
# Con NOBUILD=1 no recompila el software (reusa el ram.hex actual).
# ELAB_OPTS pasa opciones a la elaboracion, p. ej. parametros del testbench:
#   ELAB_OPTS=-gLTC=1 scripts/sim-tb.sh adc
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

GEN="$HERE/hw/soc/gen/simulation"
[ -f "$GEN/mentor/msim_setup.tcl" ] || { echo "ERROR: corra primero scripts/build-soc.sh" >&2; exit 1; }
HEX="$HERE/build/niosv-test_$NAME/app/build/ram.hex"
[ -f "$HEX" ] || { echo "ERROR: falta $HEX" >&2; exit 1; }
mkdir -p "$HERE/build/sim"
# Las opciones de elaboracion forman parte del nombre: la misma prueba con distintos
# parametros (p. ej. tb_adc con -gLTC=1) puede correr en paralelo sin pisarse.
SUFFIX="$(printf '%s' "${ELAB_OPTS:-}" | tr -c 'A-Za-z0-9' '_')"
LOG="$HERE/build/sim/tb_$NAME${SUFFIX:+_$SUFFIX}.log"

# Directorio de simulacion propio de este testbench: las fuentes generadas se
# enlazan (enlaces duros, solo se leen) y mentor/ es una copia, porque ahi Questa
# compila sus librerias y lee ram.hex. Asi varios testbenches corren en paralelo.
WORK="$HERE/build/sim/tb_$NAME${SUFFIX:+_$SUFFIX}.d"
rm -rf "$WORK"
mkdir -p "$WORK/mentor"
cp -al "$GEN/submodules" "$WORK/"
cp -al "$GEN"/*.v "$WORK/" 2>/dev/null || true
cp "$GEN/mentor/msim_setup.tcl" "$WORK/mentor/"
# Los .mif con varios valores por linea (la fuente del Character Buffer) se expanden
# en la copia de simulacion: el modelo altsyncram solo lee el primer valor de cada
# linea (ver scripts/expand-mif.py). Se rompe el enlace duro antes de reescribir.
for mif in "$WORK"/submodules/*.mif; do
    cp --remove-destination "$(readlink -f "$mif")" "$mif.tmp" && mv "$mif.tmp" "$mif"
    python3 -I "$HERE/scripts/expand-mif.py" "$mif" > /dev/null
done
cp "$HEX" "$WORK/mentor/ram.hex"
cd "$WORK/mentor"
# TOP_LEVEL_NAME antes de msim_setup.tcl: se elabora nuestro testbench en vez del
# audio_soc_tb que generaba Platform Designer; audio_soc sale del modelo de simulacion.
# Los modelos de perifericos externos (sim/questa/*_model.sv) se compilan siempre.
MODELS="$(ls "$HERE"/sim/questa/*_model.sv 2>/dev/null | tr '\n' ' ')"
vsim -c -do "set TOP_LEVEL_NAME tb_$NAME; set USER_DEFINED_ELAB_OPTIONS {${ELAB_OPTS:-}}; source msim_setup.tcl; file_copy; dev_com; com; \
vlog -sv -work work $MODELS $TB; elab; run -all; quit -f" > "$LOG" 2>&1 || true

grep -E "^# (TB_|\[|Respuesta|Pares|Relleno|Escrituras|OK |FILTRO |CICLOS |\*\* (Error|Fatal))|FALLO" "$LOG" | sed 's/^# //'
if grep -q "TB_[A-Z_]*: PASS" "$LOG"; then
    exit 0
fi
echo "Ver transcript completo: $LOG" >&2
exit 1
