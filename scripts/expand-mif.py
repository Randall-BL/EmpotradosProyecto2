#!/usr/bin/env python3
"""Reescribe un .mif con "dir : v0 v1 v2 ... ;" a un valor por direccion.

El formato con varios valores por linea es valido para Quartus (lo usa la ROM de la
fuente del Character Buffer, altera_up_video_char_mode_rom_128.mif), pero el modelo
altsyncram de altera_mf en Questa solo toma el primer valor de cada linea al
convertirlo a .ver: en simulacion la fuente queda casi vacia y no se ve texto.
scripts/sim-tb.sh lo aplica a la copia de simulacion; los archivos generados por
Platform Designer y la sintesis no se tocan.

Uso: python3 scripts/expand-mif.py archivo.mif [...]   (reescribe en sitio)
"""
import re
import sys

LINE = re.compile(r"^(\s*)(\w+)\s*:\s*([^;]*);(.*)$")


def expand(path):
    text = open(path, encoding="latin-1").read().splitlines()
    radix = "UNS"
    for l in text:
        m = re.match(r"\s*ADDRESS_RADIX\s*=\s*(\w+)", l)
        if m:
            radix = m.group(1).upper()
    base = {"HEX": 16, "UNS": 10, "DEC": 10, "BIN": 2, "OCT": 8}.get(radix, 10)
    fmt = {16: "{:X}", 10: "{}", 2: "{:b}", 8: "{:o}"}[base]
    out, changed, inside = [], 0, False
    for l in text:
        if re.match(r"\s*CONTENT\s+BEGIN", l):
            inside = True
        elif re.match(r"\s*END\s*;", l):
            inside = False
        m = LINE.match(l) if inside else None
        if m and "[" not in m.group(2) and len(m.group(3).split()) > 1:
            addr = int(m.group(2), base)
            for i, v in enumerate(m.group(3).split()):
                out.append(f"{m.group(1)}{fmt.format(addr + i)} : {v};")
            changed += 1
        else:
            out.append(l)
    if changed:
        open(path, "w", encoding="latin-1").write("\n".join(out) + "\n")
    return changed


if __name__ == "__main__":
    for p in sys.argv[1:]:
        n = expand(p)
        if n:
            print(f"{p}: {n} lineas expandidas")
