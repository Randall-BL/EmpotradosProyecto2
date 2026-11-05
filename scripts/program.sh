#!/usr/bin/env bash
# Programa la FPGA de la DE1-SoC por JTAG (USB-Blaster II integrado) (HW-22).
#
# En la cadena JTAG de la DE1-SoC el HPS es el dispositivo 1 y la FPGA el 2, por
# eso el .sof va a "@2". El cable se detecta solo; si hay varios, se elige con
# CABLE="DE-SoC [1-1]" scripts/program.sh.
#
# Uso:  scripts/program.sh [archivo.sof]   (por defecto el del proyecto)
#
# Alternativa durante el desarrollo: con la FPGA ya programada, cargar solo el
# programa por JTAG sin tocar el .sof:
#   niosv-download -g -r build/niosv/app/build/audio_player.elf
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh" >/dev/null
SOF="${1:-$HERE/hw/quartus/output_files/audio_player.sof}"
[ -f "$SOF" ] || { echo "ERROR: no existe $SOF" >&2; exit 1; }

if [ -z "${CABLE:-}" ]; then
    CABLE="$(quartus_pgm -l 2>/dev/null | sed -n 's/^[0-9]*) \(.*DE-SoC.*\)$/\1/p' | head -1)"
    [ -n "$CABLE" ] || CABLE="$(quartus_pgm -l 2>/dev/null | sed -n 's/^1) \(.*\)$/\1/p')"
fi
[ -n "$CABLE" ] || { echo "ERROR: no se detecto ningun cable JTAG (conecte la DE1-SoC por USB-Blaster)" >&2; exit 1; }

echo ">> Programando $SOF en '$CABLE' (dispositivo 2: FPGA) ..."
quartus_pgm -c "$CABLE" -m jtag -o "p;$SOF@2"
