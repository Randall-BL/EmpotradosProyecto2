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
niosv-bsp --create --type=hal --sopcinfo="$SOPC" "$OUT/bsp/settings.bsp"

echo ">> Generando proyecto de aplicacion (CMake) ..."
niosv-app --app-dir="$OUT/app" --bsp-dir="$OUT/bsp" \
          --srcs="$HERE/sw/niosv/src" --elf-name=audio_player.elf

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

echo ">> Listo. ELF en: $OUT/app/build/audio_player.elf"
