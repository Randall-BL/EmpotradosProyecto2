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
# En ambos casos se compilan tambien los drivers (sw/niosv/drivers) y la logica
# portable (sw/common/src, headers en sw/common/include), con -std=c11 -Wall -Wextra
# (NV-02). WERROR=1 convierte las advertencias en error.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"
cd "$HERE"

APP="${APP:-}"
SOPC="$HERE/hw/soc/audio_soc.sopcinfo"
# Cada programa compila en su carpeta (build/niosv para la aplicacion,
# build/niosv-<APP> para las pruebas): asi varias simulaciones corren en paralelo.
OUT="$HERE/build/niosv${APP:+-$APP}"
if [ ! -f "$SOPC" ]; then
    echo "ERROR: falta $SOPC. Corra primero scripts/build-soc.sh" >&2
    exit 1
fi

# Partimos de cero para que el BSP refleje cambios del .qsys (p. ej. PIO nuevos).
# Solo se borra la carpeta de este programa: build/ tambien guarda la salida del HPS.
rm -rf "$OUT"
mkdir -p "$OUT"

echo ">> Generando BSP (HAL) ..."
niosv-bsp --create --type=hal --sopcinfo="$SOPC" --no-default \
          --script="$HERE/sw/niosv/bsp.tcl" "$OUT/bsp/settings.bsp"

if [ -n "${APP:-}" ]; then
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
COMMON="$HERE/sw/common"

echo ">> Generando proyecto de aplicacion (CMake): $SRC ..."
niosv-app --app-dir="$OUT/app" --bsp-dir="$OUT/bsp" \
          --srcs="$SRC,$DRV,$COMMON/src" --incs="$DRV,$COMMON/include" --elf-name="$ELF"

# NV-02: el codigo propio (aplicacion, drivers y sw/common) se compila con C11 y todas
# las advertencias. Los headers del BSP generado (HAL) se marcan como de sistema
# (-isystem): sus advertencias no son nuestras y no deben tapar las propias. irq.c,
# el unico que incluye el HAL (sys/alt_irq.h, con extensiones de GNU), va con gnu11.
# Con WERROR=1 cualquier advertencia del codigo propio corta la compilacion.
cat >> "$OUT/app/CMakeLists.txt" <<CMAKE

# --- agregado por scripts/build-niosv.sh (NV-02) ---
set_target_properties(\${BspLibraryName} PROPERTIES SYSTEM TRUE)
target_compile_options($ELF PRIVATE -std=c11 -Wall -Wextra \$<\$<BOOL:${WERROR:-0}>:-Werror>)
set_source_files_properties("$DRV/irq.c" PROPERTIES COMPILE_OPTIONS "-std=gnu11")
CMAKE

echo ">> Compilando ..."
# Release (-O2): el CMake que genera niosv-app usa Debug por defecto (solo -g, sin
# optimizar) y el codigo corria varias veces mas lento (medido con tb_ciclos).
cmake -G "Unix Makefiles" -S "$OUT/app" -B "$OUT/app/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
make -C "$OUT/app/build"

# La RAM on-chip en simulacion lee "ram.hex" desde el directorio donde corre
# vsim (hw/soc/gen/simulation/mentor). Copiamos la imagen recien generada ahi.
MENTOR="$HERE/hw/soc/gen/simulation/mentor"
if [ -z "$APP" ] && [ -d "$MENTOR" ]; then
    cp "$OUT/app/build/ram.hex" "$MENTOR/ram.hex"
    echo ">> ram.hex copiado a $MENTOR"
fi

echo ">> Listo. ELF en: $OUT/app/build/$ELF"
