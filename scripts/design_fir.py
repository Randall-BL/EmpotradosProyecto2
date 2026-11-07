#!/usr/bin/env python3
"""Diseno, cuantizacion y vectores de prueba del filtro FIR del modulo propio (HW-12).

Es la fuente de verdad numerica del filtro: lo que genera lo usan el HDL, el driver,
el modelo en C y los testbenches. Con fs = 48 kHz fija (CD-07) y el formato de punto
fijo de CD-14:

  muestras      Q1.15 con signo (16 bits)
  coeficientes  Q1.15 con signo (16 bits)
  acumulador    40 bits (16 + 16 + log2(128) = 39, mas uno de guarda)
  salida        y = sat16((acc + 2^14) >> 15)         redondeo al mas cercano
  ganancia      z = sat16((y * g + 2^14) >> 15)        g Q1.15 sin signo, 0..65535 (0..2.0)
  bypass        y = x (la linea de retardo se sigue actualizando)

Bancos de coeficientes (hasta 128 taps cada uno):
  0  pasa-bajos FIR, fc = 1 kHz, 127 taps, ventana de Kaiser   (filtro obligatorio en HDL)
  1  pasa-banda FIR "voz", 1-4 kHz, 127 taps                      (segundo filtro, mismo HW)
     Con 127 taps a 48 kHz la transicion es de ~1.4 kHz: un borde inferior en 300 Hz no
     se puede realizar (dejaba pasar 31 % en continua), por eso empieza en 1 kHz.

Salidas (con --out, por defecto en el repositorio):
  hw/rtl/fir_coefs.hex         256 coeficientes (banco 0 y 1) para $readmemh del RTL
  sw/common/fir_coefs.h        los mismos para C (driver NV-10 y modelo SC-10)
  sim/vectors/fir_*.txt        casos de prueba con la salida esperada bit a bit
  docs/img/fir_respuesta.png   respuesta ideal frente a cuantizada
  y el analisis de error de CD-14 por la salida estandar.

Uso: python3 scripts/design_fir.py
"""
import argparse
import pathlib

import numpy as np
from scipy import signal

ROOT = pathlib.Path(__file__).resolve().parent.parent
FS = 48000
NTAPS = 127
MAX_TAPS = 128
Q = 15


# ---------------------------------------------------------------- diseno
def design():
    lp = signal.firwin(NTAPS, 1000, window=("kaiser", 6.0), fs=FS)
    bp = signal.firwin(NTAPS, [1000, 4000], pass_zero=False, window=("kaiser", 6.0), fs=FS)
    return {"pasa-bajos 1 kHz": lp, "pasa-banda 1-4 kHz": bp}


def quantize(h):
    q = np.round(h * (1 << Q)).astype(np.int64)
    return np.clip(q, -(1 << Q), (1 << Q) - 1)


# ------------------------------------------------- modelo bit a bit del HW
def sat16(v):
    return max(-32768, min(32767, int(v)))


class FirModel:
    """Modelo exacto del datapath de hw/rtl/fir_filter.v (un canal)."""

    def __init__(self, coefs, ntaps):
        self.h = [int(c) for c in coefs[:ntaps]]
        self.ntaps = ntaps
        self.dl = [0] * MAX_TAPS
        self.sat = False

    def step(self, x, bypass=False, gain=32768):
        self.dl = [int(x)] + self.dl[:-1]
        if bypass:
            y = int(x)
        else:
            acc = sum(self.h[k] * self.dl[k] for k in range(self.ntaps))
            r = (acc + (1 << (Q - 1))) >> Q          # >> de Python = desplazamiento aritmetico
            y = sat16(r)
            self.sat |= (y != r)
        z = (y * gain + (1 << (Q - 1))) >> Q
        zs = sat16(z)
        self.sat |= (zs != z)
        return zs


# ------------------------------------------------------------ analisis CD-14
def analysis(name, h, hq):
    w, H = signal.freqz(h, worN=8192, fs=FS)
    _, Hq = signal.freqz(hq / (1 << Q), worN=8192, fs=FS)
    err_db = 20 * np.log10(np.abs(H - Hq).max() + 1e-12)
    if "bajos" in name:
        sb = np.abs(Hq)[(w > 2400) & (w < 24000)]
    else:
        sb = np.abs(Hq)[(w < 250) | (w > 5600)]
    print(f"  {name}:")
    print(f"    suma de coeficientes (ganancia DC) = {hq.sum() / (1 << Q):.5f}")
    print(f"    suma |h| (ganancia maxima posible) = {np.abs(hq).sum() / (1 << Q):.4f}"
          f"  -> {'puede saturar' if np.abs(hq).sum() > (1 << Q) else 'no satura'}")
    print(f"    coeficiente maximo |h| = {np.abs(hq).max()} / 32767")
    print(f"    error maximo |H - Hq| = {err_db:.1f} dB respecto a ganancia 1")
    print(f"    atenuacion minima en la banda de rechazo (cuantizado) = "
          f"{-20 * np.log10(sb.max()):.1f} dB")
    return w, H, Hq


# ------------------------------------------------------------ vectores
def write_case(path, desc, bank, ntaps, gain, bypass, xs_l, xs_r, coefs):
    ml, mr = FirModel(coefs, ntaps), FirModel(coefs, ntaps)
    with open(path, "w") as f:
        f.write(f"{bank} {ntaps} {gain} {int(bypass)} {len(xs_l)}\n")
        for xl, xr in zip(xs_l, xs_r):
            yl, yr = ml.step(xl, bypass, gain), mr.step(xr, bypass, gain)
            f.write(f"{(xr & 0xFFFF) << 16 | (xl & 0xFFFF):08x} "
                    f"{(yr & 0xFFFF) << 16 | (yl & 0xFFFF):08x}\n")
    return desc, ml.sat or mr.sat


def vectors(outdir, banks):
    outdir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(1113)
    n = np.arange(200)
    lp, bp = banks
    sign = np.where(lp[:NTAPS][::-1] >= 0, 1, -1)
    worst = np.concatenate([np.zeros(10, int), np.tile(sign * 32767, 2)[:190]])
    cases = [
        ("impulso", "impulso en L (salida = coeficientes)", 0, NTAPS, 32768, False,
         [32767] + [0] * 139, [0] * 140),
        ("escalon", "escalon de 0.5 (valor final = 0.5 * suma de coeficientes)", 0, NTAPS, 32768, False,
         [16384] * 200, [-16384] * 200),
        ("senos", "L: 500 Hz (pasa), R: 8 kHz (se rechaza)", 0, NTAPS, 32768, False,
         list((16000 * np.sin(2 * np.pi * 500 * n / FS)).astype(int)),
         list((16000 * np.sin(2 * np.pi * 8000 * n / FS)).astype(int))),
        ("saturacion", "entrada con el signo de cada coeficiente: satura", 0, NTAPS, 32768, False,
         list(worst), list(-worst)),
        ("ruido_banco1", "ruido blanco por el pasa-banda (banco 1)", 1, NTAPS, 32768, False,
         list(rng.integers(-20000, 20000, 200)), list(rng.integers(-20000, 20000, 200))),
        ("bypass_gain", "bypass con ganancia 0.5", 0, NTAPS, 16384, True,
         list(rng.integers(-32768, 32767, 64)), list(rng.integers(-32768, 32767, 64))),
        ("taps16", "solo 16 taps del banco 0, ruido", 0, 16, 32768, False,
         list(rng.integers(-32768, 32767, 64)), list(rng.integers(-32768, 32767, 64))),
        ("ganancia_alta", "ganancia 1.5 sobre el pasa-bajos: satura la etapa de ganancia", 0, NTAPS, 49152, False,
         [30000] * 160, [-30000] * 160),
    ]
    index = []
    for name, desc, bank, ntaps, gain, bypass, xl, xr in cases:
        d, sat = write_case(outdir / f"fir_{name}.txt", desc, bank, ntaps, gain, bypass,
                            [int(v) for v in xl], [int(v) for v in xr], banks[bank])
        index.append((name, d, sat))
    with open(outdir / "README.md", "w") as f:
        f.write("# Vectores dorados del filtro FIR (generados por scripts/design_fir.py)\n\n")
        f.write("Primera linea: `banco taps ganancia bypass muestras`. Luego, por muestra:\n")
        f.write("`entrada salida` en hex de 32 bits = `{R[15:0], L[15:0]}`.\n\n")
        f.write("| Archivo | Caso | Satura |\n|---|---|---|\n")
        for name, d, sat in index:
            f.write(f"| `fir_{name}.txt` | {d} | {'si' if sat else 'no'} |\n")
    return index


# ------------------------------------------------------------ exportar
def export(banks_q, rtl_hex, c_header):
    flat = []
    for q in banks_q:
        flat += list(q) + [0] * (MAX_TAPS - len(q))
    rtl_hex.write_text("".join(f"{int(c) & 0xFFFF:04x}\n" for c in flat))
    lines = ["/* Generado por scripts/design_fir.py: no editar a mano. */",
             "#ifndef COMMON_FIR_COEFS_H", "#define COMMON_FIR_COEFS_H", "",
             "#include <stdint.h>", "",
             f"#define FIR_MAX_TAPS {MAX_TAPS}u", f"#define FIR_NTAPS {NTAPS}u",
             "#define FIR_BANK_LOWPASS  0u   /* pasa-bajos 1 kHz */",
             "#define FIR_BANK_BANDPASS 1u   /* pasa-banda 1-4 kHz */", "",
             f"static const int16_t fir_coefs[2][FIR_NTAPS] = {{"]
    for q in banks_q:
        lines.append("    {")
        for k in range(0, NTAPS, 10):
            lines.append("        " + ", ".join(f"{int(c):6d}" for c in q[k:k + 10]) + ",")
        lines.append("    },")
    lines += ["};", "", "#endif /* COMMON_FIR_COEFS_H */", ""]
    c_header.write_text("\n".join(lines))


def plot(path, curves):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(9, 4.5))
    for name, (w, H, Hq) in curves.items():
        ax.plot(w, 20 * np.log10(np.abs(H) + 1e-9), "--", lw=1, label=f"{name} (ideal)")
        ax.plot(w, 20 * np.log10(np.abs(Hq) + 1e-9), lw=1.4, label=f"{name} (Q1.15)")
    ax.set_xscale("log")
    ax.set_xlim(50, FS / 2)
    ax.set_ylim(-100, 5)
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_ylabel("Magnitud (dB)")
    ax.set_title(f"Filtros FIR del modulo propio: {NTAPS} taps, fs = 48 kHz")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=120)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT), help="raiz donde escribir (por defecto el repo)")
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    filters = design()
    banks_q = [quantize(h) for h in filters.values()]
    print("Analisis de punto fijo (CD-14):")
    curves = {name: analysis(name, h, q) for (name, h), q in zip(filters.items(), banks_q)}
    (out / "hw" / "rtl").mkdir(parents=True, exist_ok=True)
    (out / "sw" / "common").mkdir(parents=True, exist_ok=True)
    (out / "docs" / "img").mkdir(parents=True, exist_ok=True)
    export(banks_q, out / "hw/rtl/fir_coefs.hex", out / "sw/common/fir_coefs.h")
    plot(out / "docs/img/fir_respuesta.png", curves)
    idx = vectors(out / "sim/vectors", banks_q)
    print(f"Vectores: {len(idx)} casos en sim/vectors/ (con saturacion: "
          f"{', '.join(n for n, _, s in idx if s) or 'ninguno'})")


if __name__ == "__main__":
    main()
