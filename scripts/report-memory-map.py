#!/usr/bin/env python3
"""Mapa de memoria e interrupciones exportado de Platform Designer (HW-24).

Lee hw/soc/audio_soc.sopcinfo (lo genera scripts/build-soc.sh) y escribe en
Markdown las tablas de direcciones vistas por cada maestro Avalon (data_manager
e instruction_manager del Nios V; el HPS cuando se agregue) y de interrupciones.
Asi el mapa documentado sale del SoC real y no se desincroniza.

Uso: python3 scripts/report-memory-map.py > docs/hw/mapa-memoria-generado.md
"""
import pathlib
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOPC = ROOT / "hw" / "soc" / "audio_soc.sopcinfo"
SYSTEM_H = ROOT / "build" / "niosv" / "bsp" / "system.h"


def system_h_spans():
    """Tamanos <INSTANCIA>[_<INTERFAZ>]_SPAN del system.h del BSP, en bytes. Son la
    referencia: el .sopcinfo mezcla unidades (algunos IP declaran WORDS pero dan
    bytes)."""
    out = {}
    if SYSTEM_H.exists():
        for line in SYSTEM_H.read_text().splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[0] == "#define" and parts[1].endswith("_SPAN"):
                out[parts[1][:-5]] = int(parts[2], 0)
    return out


def params(node):
    out = {}
    for p in node.findall("parameter"):
        v = p.find("value")
        out[p.get("name")] = v.text if v is not None else None
    return out


def span_bytes(itf, p):
    """Tamano en bytes de un esclavo. explicitAddressSpan ya viene en bytes; si no,
    con addressUnits = WORDS el addressSpan esta en palabras del ancho de datos."""
    explicit = int(p.get("explicitAddressSpan") or 0)
    if explicit:
        return explicit
    span = int(p["addressSpan"])
    if p.get("addressUnits") == "WORDS":
        width = 32
        for port in itf.findall("port"):
            n, w = port.find("name"), port.find("width")
            if n is not None and w is not None and n.text.endswith("writedata"):
                width = int(w.text)
        span *= max(width // 8, 1)
    return span


def main():
    if not SOPC.exists():
        sys.exit(f"ERROR: falta {SOPC}; corra scripts/build-soc.sh")
    root = ET.parse(SOPC).getroot()

    kinds, spans = {}, {}
    for mod in root.iter("module"):
        name = mod.get("name")
        kinds[name] = mod.get("kind")
        for itf in mod.findall("interface"):
            p = params(itf)
            if "addressSpan" in p and p["addressSpan"]:
                spans[f"{name}.{itf.get('name')}"] = span_bytes(itf, p)

    hspans = system_h_spans()

    def span_of(slave):
        inst, itf = slave.split(".")
        for key in (f"{inst}_{itf}".upper(), inst.upper()):
            if key in hspans:
                return hspans[key]
        return spans.get(slave, 0)

    by_master, irqs = {}, []
    for c in root.iter("connection"):
        p = params(c)
        if c.get("kind") == "avalon":
            by_master.setdefault(c.get("start"), []).append(
                (int(p["baseAddress"], 16), c.get("end")))
        elif c.get("kind") == "interrupt":
            irqs.append((int(p.get("irqNumber", -1)), c.get("end"), c.get("start")))

    print("# Mapa de memoria exportado de Platform Designer\n")
    print(f"Generado por `scripts/report-memory-map.py` desde `{SOPC.relative_to(ROOT)}` "
          f"(tamanos de `system.h` del BSP).\n")
    for master in sorted(by_master):
        print(f"## Visto desde `{master}`\n")
        print("| Base | Fin | Tamaño | Esclavo | Componente |")
        print("|---|---|---|---|---|")
        for base, slave in sorted(by_master[master]):
            span = span_of(slave)
            end = base + span - 1 if span else base
            size = f"{span // 1024} KB" if span >= 1024 else f"{span} B"
            inst = slave.split(".")[0]
            print(f"| `0x{base:08X}` | `0x{end:08X}` | {size} | `{slave}` | {kinds.get(inst, '')} |")
        print()
    print("## Interrupciones\n")
    print("| IRQ | Fuente | Receptor |")
    print("|---|---|---|")
    for num, src, dst in sorted(irqs):
        print(f"| {num} | `{src}` | `{dst}` |")


if __name__ == "__main__":
    main()
