#!/usr/bin/env bash
# Compila el software para el Nios V/m:
#   - BSP HAL a partir de hw/soc/audio_soc.sopcinfo
#   - aplicacion (proyecto CMake) a partir de sw/niosv/src/
#   - enlaza audio_player.elf y genera ram.hex (elf2hex), que es la imagen con
#     que se inicializa la RAM on-chip en simulacion.
#
# Origen: Taller 9, scripts/build-sw.sh (ver docs/reutilizado-taller9.md).
#
# Requiere haber corrido antes:  scripts/build-soc.sh
#
# Uso:
#   scripts/build-niosv.sh              # aplicacion del reproductor (sw/niosv/src)
#   APP=test_pio scripts/build-niosv.sh # programa de prueba sw/niosv/tests/test_pio
# En ambos casos se compilan tambien los drivers de sw/niosv/drivers.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"
cd "$HERE"

SOPC="$HERE/hw/soc/audio_soc.sopcinfo"
OUT="$HERE/build/niosv"
if [ ! -f "$SOPC" ]; then
    echo "ERROR: falta $SOPC. Corra primero scripts/build-soc.sh" >&2
    exit 1
fi

# Partimos de cero para que el BSP refleje cambios del .qsys (p. ej. PIO nuevos).
# Solo se borra build/niosv: build/ tambien guarda la salida del HPS.
rm -rf "$OUT"
mkdir -p "$OUT"

echo ">> Generando BSP (HAL) ..."
niosv-bsp --create --type=hal --sopcinfo="$SOPC" --no-default \
          --script="$HERE/sw/niosv/bsp.tcl" "$OUT/bsp/settings.bsp"

APP="${APP:-}"
if [ -n "$APP" ]; then
    SRC="$HERE/sw/niosv/tests/$APP"
    ELF="$APP.elf"
else
    SRC="$HERE/sw/niosv/src"
    ELF="audio_player.elf"
fi
if [ ! -d "$SRC" ]; then
    echo "ERROR: no existe $SRC" >&2
    exit 1
fi
DRV="$HERE/sw/niosv/drivers"

echo ">> Generando proyecto de aplicacion (CMake): $SRC ..."
niosv-app --app-dir="$OUT/app" --bsp-dir="$OUT/bsp" \
          --srcs="$SRC,$DRV" --incs="$DRV" --elf-name="$ELF"

echo ">> Compilando ..."
cmake -G "Unix Makefiles" -S "$OUT/app" -B "$OUT/app/build" >/dev/null
make -C "$OUT/app/build"

# La RAM on-chip en simulacion lee "ram.hex" desde el directorio donde corre
# vsim (hw/soc/gen/testbench/mentor). Copiamos la imagen recien generada ahi.
MENTOR="$HERE/hw/soc/gen/testbench/mentor"
if [ -d "$MENTOR" ]; then
    cp "$OUT/app/build/ram.hex" "$MENTOR/ram.hex"
    echo ">> ram.hex copiado a $MENTOR"
fi

echo ">> Listo. ELF en: $OUT/app/build/$ELF"
