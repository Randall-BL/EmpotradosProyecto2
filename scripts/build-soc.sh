#!/usr/bin/env bash
# Genera el hardware del SoC a partir de hw/soc/audio_soc.qsys:
#   - RTL de sintesis + audio_soc.sopcinfo (para el BSP)
#   - testbench de simulacion (Verilog) para Questa
#
# Origen: Taller 9, scripts/build-hw.sh (ver docs/reutilizado-taller9.md).
#
# Uso:
#   scripts/build-soc.sh               # regenera el .qsys si falta o si el Tcl es mas nuevo
#   REGEN_QSYS=1 scripts/build-soc.sh  # fuerza regenerar el .qsys desde el Tcl
#
# El .qsys no se versiona: lo que se cambie en la GUI de Platform Designer hay
# que pasarlo a build_system.tcl, o se pierde en la proxima regeneracion.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$HERE/scripts/env.sh"

SYS=audio_soc
HW="$HERE/hw/soc"
cd "$HW"

# build_system.tcl es la descripcion del SoC; el .qsys se deriva de el.
# Al regenerarlo se le copia la fecha del Tcl (qsys-generate no lo modifica), asi:
#   - Tcl mas nuevo  -> cambio el Tcl (p. ej. tras un git pull): se regenera.
#   - .qsys mas nuevo -> alguien lo edito en la GUI: no se pisa, pero se avisa.
if [ ! -f "$SYS.qsys" ] || [ "${REGEN_QSYS:-0}" = "1" ] || [ build_system.tcl -nt "$SYS.qsys" ]; then
    echo ">> Generando $SYS.qsys desde build_system.tcl ..."
    qsys-script --script=build_system.tcl
    touch -r build_system.tcl "$SYS.qsys"
elif [ "$SYS.qsys" -nt build_system.tcl ]; then
    echo "AVISO: $SYS.qsys es mas nuevo que build_system.tcl. Si lo edito en la GUI," >&2
    echo "       pase los cambios al Tcl; REGEN_QSYS=1 los descarta." >&2
fi

echo ">> Generando RTL de sintesis + .sopcinfo ..."
qsys-generate "$SYS.qsys" --synthesis=VERILOG --output-directory="$HW/gen"

echo ">> Generando testbench de simulacion (Verilog) ..."
qsys-generate "$SYS.qsys" --testbench=STANDARD --testbench-simulation=VERILOG \
              --output-directory="$HW/gen"

echo ">> Listo. .sopcinfo en: $HW/$SYS.sopcinfo"
