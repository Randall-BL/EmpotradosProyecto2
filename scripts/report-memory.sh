#!/usr/bin/env bash
# Uso de la memoria del Nios V (NV-16): tamano de cada seccion del ELF frente a
# la RAM on-chip de 128 KB (CD-06), lo que queda para pila y heap, y los
# simbolos mas grandes (para saber que optimizar).
#
# Uso:  scripts/report-memory.sh [archivo.elf]   (por defecto la aplicacion)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh" >/dev/null
ELF="${1:-$HERE/build/niosv/app/build/audio_player.elf}"
[ -f "$ELF" ] || { echo "ERROR: no existe $ELF; corra scripts/build-niosv.sh" >&2; exit 1; }
RAM=131072

echo "## Secciones de $(basename "$ELF")"
echo
echo "| Seccion | Bytes |"
echo "|---|---|"
# Solo las secciones que ocupan la RAM (sin depuracion ni metadatos).
riscv32-unknown-elf-size -A "$ELF" | \
    awk '$1 ~ /^\.(entry|exceptions|text|rodata|rwdata|sdata|sbss|bss)/ && $2 > 0 {printf "| `%s` | %d |\n", $1, $2}'
read -r TEXT DATA BSS _ < <(riscv32-unknown-elf-size "$ELF" | awk 'NR==2 {print $1, $2, $3}')
USED=$((TEXT + DATA + BSS))
echo
echo "Codigo y datos estaticos: $USED de $RAM bytes ($((USED * 100 / RAM)) %)."
echo "Libre para pila y heap: $((RAM - USED)) bytes."
echo
echo "## Simbolos mas grandes"
echo
echo "| Simbolo | Bytes | Tipo |"
echo "|---|---|---|"
riscv32-unknown-elf-nm --size-sort -S -r "$ELF" | \
    awk 'NR <= 12 {printf "| `%s` | %d | %s |\n", $4, strtonum("0x" $2), $3}'
