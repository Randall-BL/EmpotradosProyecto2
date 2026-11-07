#!/usr/bin/env python3
"""Grafica entrada y salida de casos de prueba del filtro (evidencia de PR-03).

La salida graficada es la de los vectores dorados, que el hardware reproduce bit a
bit (tb_filtro_audio); asi la figura muestra lo que hace el modulo propio.

Uso: python3 scripts/plot_vectors.py sim/vectors/fir_senos.txt docs/img/filtro-senos.png
"""
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def main():
    src, dst = sys.argv[1], sys.argv[2]
    with open(src) as f:
        bank, ntaps, gain, bypass, n = map(int, f.readline().split())
        rows = [l.split() for l in f]
    xin = [int(a, 16) for a, _ in rows]
    yout = [int(b, 16) for _, b in rows]
    fig, ax = plt.subplots(2, 1, figsize=(9, 5), sharex=True)
    for k, (name, sh) in enumerate((("L", 0), ("R", 16))):
        ax[k].plot([s16((v >> sh) & 0xFFFF) for v in xin], lw=1, label="entrada")
        ax[k].plot([s16((v >> sh) & 0xFFFF) for v in yout], lw=1.4, label="salida del hardware")
        ax[k].set_ylabel(f"Canal {name}")
        ax[k].grid(alpha=0.3)
        ax[k].legend(fontsize=8, loc="upper right")
    ax[1].set_xlabel("Muestra (fs = 48 kHz)")
    fig.suptitle(f"{src.split('/')[-1]}: banco {bank}, {ntaps} taps, ganancia {gain / 32768:.2f}")
    fig.tight_layout()
    fig.savefig(dst, dpi=110)
    print(dst)


if __name__ == "__main__":
    main()
