#!/usr/bin/env bash
# Entorno de herramientas nativas del Proyecto II (Quartus + Platform Designer +
# Nios V + Questa). Origen: Taller 9, scripts/env.sh (ver docs/reutilizado-taller9.md).
#
# Uso:  source scripts/env.sh
#
# Ajuste ALTERA_ROOT si su instalacion de Quartus/Altera esta en otra ruta.
# Ajuste SALT_LICENSE_SERVER si su licencia de Questa esta en otro lugar.

: "${ALTERA_ROOT:=/opt/altera_lite/25.1std}"

if [ ! -d "$ALTERA_ROOT/quartus" ]; then
    echo "ERROR: no encuentro Quartus en '$ALTERA_ROOT/quartus'." >&2
    echo "       Exporte ALTERA_ROOT apuntando a su instalacion, p. ej.:" >&2
    echo "       export ALTERA_ROOT=/ruta/a/altera_lite/25.1std" >&2
    return 1 2>/dev/null || exit 1
fi

export QUARTUS_ROOTDIR="$ALTERA_ROOT/quartus"
export QSYS_ROOTDIR="$QUARTUS_ROOTDIR/sopc_builder/bin"
export PATH="$QUARTUS_ROOTDIR/bin:$QUARTUS_ROOTDIR/sopc_builder/bin:$ALTERA_ROOT/niosv/bin:$ALTERA_ROOT/riscfree/toolchain/riscv32-unknown-elf/bin:$ALTERA_ROOT/questa_fse/bin:$PATH"

# Questa Altera Starter usa licenciamiento SALT (no FLEXlm): la variable es
# SALT_LICENSE_SERVER, NO LM_LICENSE_FILE.
: "${SALT_LICENSE_SERVER:=$HOME/altera_lite/LR-203954_License.dat}"
export SALT_LICENSE_SERVER

echo "Entorno Quartus/Nios V listo:"
echo "  ALTERA_ROOT         = $ALTERA_ROOT"
echo "  SALT_LICENSE_SERVER = $SALT_LICENSE_SERVER"
