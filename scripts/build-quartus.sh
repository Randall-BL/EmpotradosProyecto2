#!/usr/bin/env bash
# Compila el proyecto Quartus del reproductor (HW-21): sintesis, fitter,
# ensamblador (.sof) y Timing Analyzer.
#
# Requiere scripts/build-soc.sh (RTL del SoC) y scripts/build-niosv.sh (ram.hex,
# que queda dentro del .sof como contenido inicial de la RAM del Nios V).
#
# Uso:  scripts/build-quartus.sh
# Salida: hw/quartus/output_files/audio_player.sof y los reportes *.rpt.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh" >/dev/null
QDIR="$HERE/hw/quartus"
HEX="$HERE/build/niosv/app/build/ram.hex"

[ -f "$HERE/hw/soc/gen/synthesis/audio_soc.qip" ] || { echo "ERROR: corra primero scripts/build-soc.sh" >&2; exit 1; }
[ -f "$HEX" ] || { echo "ERROR: falta $HEX; corra primero scripts/build-niosv.sh" >&2; exit 1; }

# La RAM on-chip busca ram.hex en el directorio del proyecto Quartus.
cp "$HEX" "$QDIR/ram.hex"

cd "$QDIR"
echo ">> Compilando audio_player (puede tardar varios minutos) ..."
quartus_sh --flow compile audio_player > "$QDIR/output_files.log" 2>&1 || {
    grep -E "^Error" "$QDIR/output_files.log" | head -20 >&2
    echo "ERROR: fallo la compilacion, ver hw/quartus/output_files.log" >&2
    exit 1
}

STA="$QDIR/output_files/audio_player.sta.rpt"
echo ">> Resumen:"
grep -E "Total logic elements|Logic utilization|Total registers|Total block memory bits|Total DSP Blocks|Total pins" \
    "$QDIR/output_files/audio_player.fit.summary" | sed 's/^/   /'
echo ">> Slack por reloj (Timing Analyzer, peor de las 4 esquinas; setup, hold, recovery, removal):"
awk '/; Multicorner Timing Analysis Summary/,/^$/' "$STA" | \
    awk -F';' '/Worst-case Slack|CLOCK_50|altera_reserved_tck|divclk/ {gsub(/ +/," "); print "  " $2 ":" $3 $4 $5 $6}' | head -6
if awk '/; Multicorner Timing Analysis Summary/,/^$/' "$STA" | grep -E "Worst-case Slack" | grep -q -- " -[0-9]"; then
    echo "AVISO: hay requisitos de tiempo sin cumplir (ver $STA)" >&2
    exit 2
fi
# HP-04: .rbf comprimido para que U-Boot configure la FPGA desde la SD
# (SW10/MSEL = 01010, FPP x16 con compresion).
quartus_cpf -c -o bitstream_compression=on \
    "$QDIR/output_files/audio_player.sof" "$QDIR/output_files/audio_player.rbf" >/dev/null
echo ">> Listo: $QDIR/output_files/audio_player.sof y audio_player.rbf"
