#!/usr/bin/env python3
"""Extrae la utilizacion de recursos de los reportes de Quartus (RP-01, RP-02).

Lee hw/quartus/output_files/audio_player.fit.summary (totales) y la tabla
"Fitter Resource Utilization by Entity" de audio_player.fit.rpt (por componente
del SoC) y escribe tablas en Markdown.

Uso:
  python3 scripts/report-resources.py                 # sistema completo + por componente
  python3 scripts/report-resources.py --entity filtro # ademas, el detalle de un modulo
                                                      # (nombre de instancia o entidad)
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "hw" / "quartus" / "output_files"
COLS = ["ALMs needed [=A-B+C]", "Dedicated Logic Registers", "Block Memory Bits", "M10Ks", "DSP Blocks"]
SHORT = ["ALMs", "Registros", "Bits de memoria", "M10K", "DSP"]


def summary():
    rows = []
    for line in (OUT / "audio_player.fit.summary").read_text(encoding="latin-1").splitlines():
        m = re.match(r"\s*(Logic utilization \(in ALMs\)|Total registers|Total pins|"
                     r"Total block memory bits|Total RAM Blocks|Total DSP Blocks|Total PLLs)\s*:\s*(.+)", line)
        if m:
            rows.append((m.group(1), m.group(2).strip()))
    return rows


def entities():
    text = (OUT / "audio_player.fit.rpt").read_text(encoding="latin-1").splitlines()
    start = next(i for i, l in enumerate(text) if l.startswith("; Fitter Resource Utilization by Entity"))
    header = [c.strip() for c in text[start + 2].split(";")[1:-1]]
    idx = [header.index(c) for c in COLS]
    i_name = header.index("Compilation Hierarchy Node")
    i_ent = header.index("Entity Name")
    out = []
    for l in text[start + 4:]:
        if not l.startswith(";"):
            break
        cells = l.split(";")[1:-1]
        raw = cells[i_name]
        depth = (len(raw) - len(raw.lstrip(" "))) // 3
        name = raw.strip().strip("|")
        vals = [re.sub(r"\s*\(.*\)", "", cells[k].strip()) for k in idx]
        out.append((depth, name, cells[i_ent].strip(), vals))
    return out


def md_table(head, rows):
    s = "| " + " | ".join(head) + " |\n|" + "---|" * len(head) + "\n"
    for r in rows:
        s += "| " + " | ".join(str(c) for c in r) + " |\n"
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--entity", help="instancia o entidad a detallar (RP-02)")
    a = ap.parse_args()
    if not (OUT / "audio_player.fit.rpt").exists():
        sys.exit("ERROR: falta audio_player.fit.rpt; corra scripts/build-quartus.sh")

    print("## Sistema completo\n")
    print(md_table(["Recurso", "Uso"], summary()))

    ents = entities()
    print("## Por componente del SoC (instancias de audio_soc)\n")
    rows = []
    for depth, name, ent, vals in ents:
        if depth == 2:                              # hijos de audio_soc:u_soc
            inst = name.split(":")[-1]
            rows.append([f"`{inst}`", ent] + vals)
        elif depth == 0:
            top = vals
    rows.sort(key=lambda r: -float(r[2] or 0))
    print(md_table(["Instancia", "Entidad"] + SHORT, rows))
    print(f"Total del top-level (incluye el reset de encendido): {top[0]} ALMs, {top[1]} registros, "
          f"{top[2]} bits de memoria, {top[3]} M10K, {top[4]} DSP.\n")

    if a.entity:
        sel = [e for e in ents if a.entity in e[1] or a.entity in e[2]]
        if not sel:
            sys.exit(f"ERROR: no hay ninguna instancia o entidad con '{a.entity}'")
        d0 = sel[0][0]
        print(f"## Detalle de `{a.entity}`\n")
        print(md_table(["Nodo", "Entidad"] + SHORT,
                       [["&nbsp;" * 2 * (d - d0) + f"`{n.split(':')[-1]}`", e] + v for d, n, e, v in sel]))


if __name__ == "__main__":
    main()
