#!/usr/bin/env python3
"""Convierte la captura PBM (P1) de tb_vga a PNG, sin dependencias externas.

Pinta el texto en blanco sobre fondo negro, como lo saca el Character Buffer, y
puede escalar la imagen para que se lea mejor en el README.

Uso: python3 scripts/pbm2png.py build/sim/vga_pantalla.pbm docs/img/vga-simulada.png [escala]
"""
import struct
import sys
import zlib


def read_pbm(path):
    """Lee un PBM de texto (P1) y devuelve (ancho, alto, filas de booleanos)."""
    tok = open(path).read().split()
    assert tok[0] == "P1", "se esperaba PBM de texto (P1)"
    w, h = int(tok[1]), int(tok[2])
    bits = "".join(tok[3:])
    return w, h, [[bits[y * w + x] == "1" for x in range(w)] for y in range(h)]


def write_png(path, w, h, rows):
    """Escribe un PNG en escala de grises de 8 bits sin bibliotecas externas.

    Arma a mano la firma y los chunks IHDR, IDAT (filas con filtro 0, comprimidas
    con zlib) e IEND, cada uno con su CRC.
    """
    def chunk(t, data):
        """Chunk PNG: largo + tipo + datos + CRC32 de tipo y datos."""
        c = struct.pack(">I", len(data)) + t + data
        return c + struct.pack(">I", zlib.crc32(t + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(255 if p else 0 for p in r) for r in rows)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    """Lee src, escala cada pixel a k x k y escribe dst."""
    src, dst = sys.argv[1], sys.argv[2]
    k = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    w, h, img = read_pbm(src)
    rows = [[p for p in r for _ in range(k)] for r in img for _ in range(k)]
    write_png(dst, w * k, h * k, rows)
    print(f"{dst}: {w * k} x {h * k}")


if __name__ == "__main__":
    main()
