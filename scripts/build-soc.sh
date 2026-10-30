#!/usr/bin/env bash
# Genera el hardware del SoC a partir de hw/soc/audio_soc.qsys:
#   - RTL de sintesis + audio_soc.sopcinfo (para el BSP)
#   - testbench de simulacion (Verilog) para Questa
#
# Origen: Taller 9, scripts/build-hw.sh (ver docs/reutilizado-taller9.md).
#
# Uso:
#   scripts/build-soc.sh               # regenera el .qsys solo si no existe
#   REGEN_QSYS=1 scripts/build-soc.sh  # fuerza regenerar el .qsys desde el Tcl
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"

SYS=audio_soc
HW="$HERE/hw/soc"
cd "$HW"

# build_system.tcl es la descripcion del SoC; el .qsys se deriva de el.
if [ ! -f "$SYS.qsys" ] || [ "${REGEN_QSYS:-0}" = "1" ]; then
    echo ">> Generando $SYS.qsys desde build_system.tcl ..."
    qsys-script --script=build_system.tcl
fi

echo ">> Generando RTL de sintesis + .sopcinfo ..."
qsys-generate "$SYS.qsys" --synthesis=VERILOG --output-directory="$HW/gen"

echo ">> Generando testbench de simulacion (Verilog) ..."
qsys-generate "$SYS.qsys" --testbench=STANDARD --testbench-simulation=VERILOG \
              --output-directory="$HW/gen"

echo ">> Listo. .sopcinfo en: $HW/$SYS.sopcinfo"
