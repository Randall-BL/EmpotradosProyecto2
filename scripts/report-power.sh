#!/usr/bin/env bash
# Estimacion de potencia del sistema completo con el Power Analyzer de Quartus
# (quartus_pow) (RP-03).
#
# Sin vectores de simulacion, quartus_pow usa estimacion "vectorless": propaga una
# tasa de conmutacion supuesta desde las entradas. Se documenta la supuesta (12.5 %
# por ciclo en las entradas, el valor por defecto de Quartus) junto al resultado.
# Con VCD=archivo.vcd usa la actividad real de una simulacion (mas exacto).
#
# Requiere scripts/build-quartus.sh.  Salida: hw/quartus/output_files/audio_player.pow.*
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh" >/dev/null
cd "$HERE/hw/quartus"
[ -f output_files/audio_player.sof ] || { echo "ERROR: corra primero scripts/build-quartus.sh" >&2; exit 1; }

TOGGLE="${TOGGLE:-12.5%}"
ARGS=(--default_input_io_toggle_rate="$TOGGLE" --use_vectorless_estimation=on)
if [ -n "${VCD:-}" ]; then
    ARGS+=(--input_vcd="$VCD")
fi
quartus_pow audio_player "${ARGS[@]}" > output_files/pow.log 2>&1 \
    || { tail -20 output_files/pow.log >&2; exit 1; }
echo ">> Potencia (toggle de entradas $TOGGLE${VCD:+, VCD $VCD}):"
sed 's/^/   /' output_files/audio_player.pow.summary | grep -E "Total Thermal|Core Dynamic|Core Static|I/O Thermal|Confidence|Power Models"
