#!/usr/bin/env python3
"""Genera la tabla de seno del generador de tono de prueba (sw/common/tone.c).

1024 muestras de un periodo, en int16 con amplitud A (por debajo del maximo para
dejar margen al volumen y a los filtros). El tono se sintetiza con un acumulador
de fase de 32 bits: indice = fase >> 22, incremento = f * 2^32 / fs.

Uso: python3 scripts/gen_sine_table.py   (pegar la salida en sw/common/tone.c)
"""
import math

N = 1024
A = 16384          # -6 dBFS

print(f"/* Generada por scripts/gen_sine_table.py: {N} muestras, amplitud {A} (-6 dBFS). */")
print(f"static const int16_t sine_table[TONE_TABLE_SIZE] = {{")
vals = [round(A * math.sin(2 * math.pi * i / N)) for i in range(N)]
for k in range(0, N, 8):
    print("    " + ", ".join(f"{v:6d}" for v in vals[k:k + 8]) + ",")
print("};")
