#!/usr/bin/env bash
# Auditoria "sin HAL" del software propio (NV-17, §3-SW p.5).
#
# Busca IOWR, IORD y cualquier identificador alt_* en el codigo C de sw/niosv/ y
# sw/common/. Solo se permiten las funciones de interrupciones que autoriza el
# enunciado (alt_ic_isr_register, alt_ic_irq_enable, alt_ic_irq_disable) y el include
# de su header (sys/alt_irq.h). Los comentarios no cuentan.
#
# Uso: scripts/audit-hal.sh     (tambien desde make lint). Codigo 0 si esta limpio.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"

ALLOWED='alt_ic_isr_register|alt_ic_irq_enable|alt_ic_irq_disable|sys/alt_irq\.h'

hits="$(grep -rnE --include='*.c' --include='*.h' 'IOWR|IORD|alt_' sw/niosv sw/common || true)"
allowed=0
bad=""
while IFS= read -r line; do
    [ -z "$line" ] && continue
    code="${line#*:*:}"
    # Comentarios: lineas que empiezan con //, /* o * (cuerpo de un bloque).
    if [[ "$code" =~ ^[[:space:]]*(//|/\*|\*) ]]; then
        continue
    fi
    # Quita los usos permitidos y los comentarios al final de la linea.
    rest="$(printf '%s' "$code" | sed -E "s#//.*##; s#/\*.*##; s#$ALLOWED##g")"
    if printf '%s' "$rest" | grep -qE 'IOWR|IORD|alt_'; then
        bad+="$line"$'\n'
    else
        allowed=$((allowed + 1))
    fi
done <<< "$hits"

if [ -n "$bad" ]; then
    echo "audit-hal: FALLO, uso del HAL fuera de las interrupciones:"
    printf '%s' "$bad"
    exit 1
fi
echo "audit-hal: OK (sin IOWR/IORD; $allowed lineas con alt_* permitidas, todas de interrupciones)"
grep -rnE --include='*.c' --include='*.h' "$ALLOWED" sw/niosv sw/common | sed 's/^/  /'
