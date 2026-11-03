#!/usr/bin/env python3
"""Genera la tabla de ganancias del control de volumen (SC-11).

El potenciometro se lee con 12 bits (0..4095) y se divide en VOLUME_STEPS
escalones. El oido percibe el volumen en forma logaritmica, asi que cada escalon
sube STEP_DB decibeles; el escalon 0 es silencio y el ultimo es 0 dB (ganancia 1).
Las ganancias quedan en Q15 (32768 = 1.0) y se aplican como (muestra * g) >> 15.

Uso: python3 scripts/gen_volume_table.py > /dev/stdout
     (pegar la salida en sw/common/src/volume.c, tabla volume_gain_q15)
"""
STEPS = 32
STEP_DB = 2.0

def gains():
    out = []
    for i in range(STEPS):
        if i == 0:
            out.append(0)
            continue
        db = -(STEPS - 1 - i) * STEP_DB
        g = round(32768 * 10 ** (db / 20))
        out.append(min(g, 32767))
    return out

def main():
    g = gains()
    print(f"/* Generada por scripts/gen_volume_table.py: {STEPS} escalones de {STEP_DB:g} dB,")
    print(f" * de -{(STEPS - 2) * STEP_DB:g} dB (escalon 1) a 0 dB; el escalon 0 es silencio. */")
    print(f"static const uint16_t volume_gain_q15[VOLUME_STEPS] = {{")
    for k in range(0, STEPS, 8):
        print("    " + ", ".join(f"{v:5d}" for v in g[k:k + 8]) + ",")
    print("};")

if __name__ == "__main__":
    main()
