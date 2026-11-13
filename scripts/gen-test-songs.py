#!/usr/bin/env python3
"""Genera canciones WAV de prueba para la SD (apoyo a PR-06 e IN-05..IN-07).

Diez pistas cortas, cada una una melodia distinta (arpegio con envolvente) para
reconocer de oido cual suena, con titulo/artista/album en LIST/INFO. Cubren las
frecuencias y canales que pide el enunciado: 8, 16, 44.1 y 48 kHz, mono y estereo.
No reemplazan a la coleccion real de PR-06 (musica con licencia libre).

Uso: scripts/gen-test-songs.py <carpeta> [segundos]
Solo usa la biblioteca estandar de Python.
"""
import math
import os
import struct
import sys

# (frecuencia de muestreo, canales, nota base en Hz)
SONGS = [
    (44100, 2, 261.63), (44100, 1, 293.66), (48000, 2, 329.63), (48000, 1, 349.23),
    (16000, 2, 392.00), (16000, 1, 440.00), (8000, 2, 493.88), (8000, 1, 523.25),
    (44100, 2, 587.33), (48000, 2, 659.25),
]
PATTERN = [0, 4, 7, 12, 7, 4]          # semitonos del arpegio (acorde mayor)
NOTE_S = 0.25


def chunk(cid: bytes, body: bytes) -> bytes:
    pad = b"\x00" if len(body) % 2 else b""
    return cid + struct.pack("<I", len(body)) + body + pad


def info(title: str, artist: str, album: str) -> bytes:
    def field(fid: bytes, text: str) -> bytes:
        return chunk(fid, text.encode("latin-1") + b"\x00")
    return chunk(b"LIST", b"INFO" + field(b"INAM", title) + field(b"IART", artist) +
                 field(b"IPRD", album))


def melody(rate: int, ch: int, base: float, seconds: float) -> bytes:
    n = int(rate * seconds)
    note_len = int(rate * NOTE_S)
    out = bytearray()
    for i in range(n):
        k = (i // note_len) % len(PATTERN)
        f = base * 2 ** (PATTERN[k] / 12)
        t = (i % note_len) / note_len
        env = min(1.0, t * 20) * (1 - t) ** 0.5          # ataque corto, caida suave
        s = 0.4 * env * math.sin(2 * math.pi * f * i / rate)
        left = int(s * 32767)
        if ch == 1:
            out += struct.pack("<h", left)
        else:                                            # derecho una octava abajo
            r = 0.4 * env * math.sin(math.pi * f * i / rate)
            out += struct.pack("<hh", left, int(r * 32767))
    return bytes(out)


def main() -> None:
    out = sys.argv[1] if len(sys.argv) > 1 else "build/hps/music"
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0
    os.makedirs(out, exist_ok=True)
    for idx, (rate, ch, base) in enumerate(SONGS, start=1):
        fmt = struct.pack("<HHIIHH", 1, ch, rate, rate * ch * 2, ch * 2, 16)
        kind = "estereo" if ch == 2 else "mono"
        meta = info(f"Prueba {idx:02d} ({rate // 1000 if rate % 1000 == 0 else rate / 1000} kHz {kind})",
                    "Generador de pruebas", "Proyecto II")
        body = b"WAVE" + chunk(b"fmt ", fmt) + meta + chunk(b"data", melody(rate, ch, base, seconds))
        name = os.path.join(out, f"{idx:02d}_prueba_{rate}_{kind}.wav")
        with open(name, "wb") as fh:
            fh.write(b"RIFF" + struct.pack("<I", len(body)) + body)
        print(name)


if __name__ == "__main__":
    main()
