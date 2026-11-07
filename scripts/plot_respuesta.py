#!/usr/bin/env python3
"""Compara la respuesta en frecuencia medida del hardware (tb_filtro_respuesta) con la
teorica de los coeficientes cuantizados (PR-10, evidencia en simulacion).

Uso: python3 scripts/plot_respuesta.py   ->  docs/img/filtro-respuesta-medida.png
Imprime el error maximo entre medida y teoria en la banda de paso y si cumple el rechazo.
"""
import csv
import pathlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal

ROOT = pathlib.Path(__file__).resolve().parent.parent
FS = 48000
NAMES = ["pasa-bajos 1 kHz (banco 0)", "pasa-banda 1-4 kHz (banco 1)"]


def main():
    raw = [int(l, 16) for l in (ROOT / "hw/rtl/fir_coefs.hex").read_text().split()]
    coefs = [np.array([v - 65536 if v & 0x8000 else v for v in raw[b * 128:b * 128 + 127]]) / 32768
             for b in range(2)]
    rows = list(csv.DictReader(open(ROOT / "build/sim/filtro_respuesta.csv")))
    fig, ax = plt.subplots(figsize=(9, 4.5))
    for b in range(2):
        f = np.array([float(r["frecuencia_hz"]) for r in rows if int(r["banco"]) == b])
        g = np.array([float(r["ganancia_db"]) for r in rows if int(r["banco"]) == b])
        w, H = signal.freqz(coefs[b], worN=4096, fs=FS)
        _, Ht = signal.freqz(coefs[b], worN=f, fs=FS)
        teo = 20 * np.log10(np.abs(Ht) + 1e-9)
        line, = ax.plot(w, 20 * np.log10(np.abs(H) + 1e-9), lw=1, alpha=0.7, label=f"{NAMES[b]}: teórica")
        ax.plot(f, g, "o", ms=4, color=line.get_color(), label=f"{NAMES[b]}: medida en el RTL")
        pas = teo > -3
        print(f"{NAMES[b]}: error max en banda de paso {np.abs(g[pas] - teo[pas]).max():.2f} dB; "
              f"atenuacion minima medida fuera de banda {-g[teo < -50].max():.1f} dB")
    ax.set_xscale("log")
    ax.set_xlim(40, FS / 2)
    ax.set_ylim(-90, 5)
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_ylabel("Ganancia (dB)")
    ax.set_title("Respuesta medida del módulo propio (senos por Avalon-MM) frente a la teórica")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=7, loc="lower left")
    fig.tight_layout()
    out = ROOT / "docs/img/filtro-respuesta-medida.png"
    fig.savefig(out, dpi=120)
    print(out.relative_to(ROOT))


if __name__ == "__main__":
    main()
