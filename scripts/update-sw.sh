#!/usr/bin/env bash
# Actualiza el software del Nios V dentro del .sof SIN recompilar el hardware
# (HW-22): recompila el programa, copia ram.hex al proyecto Quartus, actualiza el
# contenido inicial de la RAM on-chip (quartus_cdb --update_mif) y regenera el
# .sof con el ensamblador. Tarda segundos en vez de los minutos del flujo completo.
#
# Requiere una compilacion completa previa (scripts/build-quartus.sh).
#
# Uso:  scripts/update-sw.sh             # aplicacion del reproductor
#       APP=test_pio scripts/update-sw.sh  # un programa de prueba (PR-05)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh" >/dev/null
QDIR="$HERE/hw/quartus"

[ -f "$QDIR/output_files/audio_player.sof" ] || { echo "ERROR: corra primero scripts/build-quartus.sh" >&2; exit 1; }

"$HERE/scripts/build-niosv.sh" > "$HERE/build/niosv-update.log" 2>&1 \
    || { echo "ERROR: fallo build-niosv.sh, ver build/niosv-update.log" >&2; exit 1; }
cp "$HERE/build/niosv/app/build/ram.hex" "$QDIR/ram.hex"

cd "$QDIR"
echo ">> Actualizando el contenido de la RAM ..."
quartus_cdb audio_player --update_mif > "$QDIR/update_mif.log" 2>&1 \
    || { tail -20 "$QDIR/update_mif.log" >&2; exit 1; }
echo ">> Generando el .sof ..."
quartus_asm audio_player >> "$QDIR/update_mif.log" 2>&1 \
    || { tail -20 "$QDIR/update_mif.log" >&2; exit 1; }
echo ">> Listo: $QDIR/output_files/audio_player.sof (programar con scripts/program.sh)"
