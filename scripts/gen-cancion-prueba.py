#!/usr/bin/env python3
"""Genera la "cancion de prueba" del reproductor en varias frecuencias de muestreo.

Uso:  python3 scripts/gen-cancion-prueba.py [carpeta]   (por defecto build/hps/music-sd)

La misma composicion (~30 s) se sintetiza a 8, 16, 44.1 y 48 kHz, estereo, PCM de 16
bits, con titulo, artista y album en LIST/INFO (INAM, IART, IPRD). Sirve para mostrar
en la tarjeta los formatos del enunciado (WAV de 16 bits a 8, 16 y 44.1 kHz; 48 kHz es
el extra que acepta el parser) y para escuchar los filtros con sonidos conocidos:

   1. pitidos solo en el canal izquierdo        6. bombo y bajo a 120 bpm
   2. pitidos solo en el canal derecho          7. ruido blanco
   3. escala de do mayor (organo)               8. arpegio que va de izquierda a derecha
   4. acorde do-mi-sol                          9. tono de 1 kHz a -6 dBFS (referencia)
   5. barrido de 50 Hz hasta el limite de la frecuencia de muestreo

Cada frecuencia se sintetiza directo (no se remuestrea), y lo que pasaria de 0.45*fs se
omite para no tener aliasing: en la copia de 8 kHz el barrido termina en 3.6 kHz y
se oye la diferencia de ancho de banda. El pico es -6 dBFS para dejar margen a los
filtros. Solo usa la biblioteca estandar: es determinista (ruido con semilla fija).
"""
import math
import os
import random
import struct
import sys

RATES = [(8000, "08"), (16000, "16"), (44100, "44"), (48000, "48")]
FIRST_NUMBER = 12               # despues de las canciones 01..10 y la prueba 11 de 24 bits
ARTIST = "CE-1113 Grupo"
ALBUM = "Pruebas de audio"
PEAK = 0.5                      # -6 dBFS
GAP = 0.3                       # silencio entre secciones (s)


def note(n: int) -> float:
    """Frecuencia de la nota MIDI n (69 = la 440 Hz)."""
    return 440.0 * 2.0 ** ((n - 69) / 12.0)


def env(i: int, n: int, fs: int, a: float = 0.01, r: float = 0.05) -> float:
    """Envolvente con ataque y relajacion lineales (s), para no tener clics."""
    t, d = i / fs, n / fs
    return min(1.0, t / a if a else 1.0, (d - t) / r if r else 1.0)


class Song:
    def __init__(self, fs: int):
        self.fs = fs
        self.nyq = 0.45 * fs
        self.left: list = []
        self.right: list = []

    def add(self, l: list, r: list) -> None:
        self.left += l
        self.right += r
        gap = int(GAP * self.fs)
        self.left += [0.0] * gap
        self.right += [0.0] * gap

    def tone(self, freqs, dur: float, harmonics=(1.0,)) -> list:
        """Suma de senos (con armonicos debajo del limite) y envolvente."""
        fs, n = self.fs, int(dur * self.fs)
        parts = [(f * (k + 1), a) for f in freqs for k, a in enumerate(harmonics)
                 if f * (k + 1) < self.nyq]
        norm = sum(a for _, a in parts) or 1.0
        return [env(i, n, fs) * sum(a * math.sin(2 * math.pi * f * i / fs) for f, a in parts)
                / norm for i in range(n)]

    def silence(self, dur: float) -> list:
        return [0.0] * int(dur * self.fs)


def compose(fs: int) -> Song:
    s = Song(fs)
    organ = (1.0, 0.5, 0.25, 0.12)

    # 1-2. Canales: 4 pitidos de 880 Hz en cada lado.
    beeps = []
    for _ in range(4):
        beeps += s.tone([880.0], 0.25) + s.silence(0.25)
    s.add(beeps, [0.0] * len(beeps))
    s.add([0.0] * len(beeps), beeps)

    # 3. Escala de do mayor, do4 a do5.
    scale = []
    for n in (60, 62, 64, 65, 67, 69, 71, 72):
        scale += s.tone([note(n)], 0.35, organ)
    s.add(scale, scale)

    # 4. Acorde do-mi-sol.
    chord = s.tone([note(60), note(64), note(67)], 2.0, organ)
    s.add(chord, chord)

    # 5. Barrido logaritmico de 50 Hz a min(16 kHz, 0.45 fs) en 6 s.
    n, f0, f1 = int(6.0 * fs), 50.0, min(16000.0, s.nyq)
    k = math.log(f1 / f0)
    sweep = [env(i, n, fs) * math.sin(2 * math.pi * f0 * (6.0 / k) *
                                      (math.exp(k * i / n) - 1.0)) for i in range(n)]
    s.add(sweep, sweep)

    # 6. Bombo (seno que baja de 150 a 45 Hz) y bajo de 55 Hz, 8 tiempos a 120 bpm.
    beat = int(0.5 * fs)
    drums = []
    for b in range(8):
        for i in range(beat):
            t = i / fs
            kick = math.exp(-t * 18) * math.sin(2 * math.pi * (45 * t + 105 * (1 - math.exp(-t * 30)) / 30))
            bass = 0.5 * math.sin(2 * math.pi * (55 if b % 4 < 2 else 73.4) * t) * min(1, (0.5 - t) / 0.05)
            drums.append(0.6 * kick + 0.4 * bass)
    s.add(drums, drums)

    # 7. Ruido blanco (semilla fija), 2 s.
    rng = random.Random(1113)
    n = int(2.0 * fs)
    noise_l = [env(i, n, fs) * rng.uniform(-1, 1) for i in range(n)]
    noise_r = [env(i, n, fs) * rng.uniform(-1, 1) for i in range(n)]
    s.add(noise_l, noise_r)

    # 8. Arpegio do-mi-sol-do que se mueve de izquierda a derecha y vuelve.
    arp_l, arp_r = [], []
    seq = (60, 64, 67, 72, 76, 72, 67, 64) * 2
    for j, m in enumerate(seq):
        pan = 0.5 - 0.5 * math.cos(2 * math.pi * j / len(seq))   # 0 = izquierda, 1 = derecha
        x = s.tone([note(m)], 0.3, organ)
        arp_l += [v * math.cos(pan * math.pi / 2) for v in x]
        arp_r += [v * math.sin(pan * math.pi / 2) for v in x]
    s.add(arp_l, arp_r)

    # 9. Referencia: 1 kHz a -6 dBFS (el pico general), 2 s.
    ref = s.tone([1000.0], 2.0)
    s.add(ref, ref)
    return s


def chunk(cid: bytes, body: bytes) -> bytes:
    data = cid + struct.pack("<I", len(body)) + body
    return data + (b"\x00" if len(body) % 2 else b"")


def info(title: str) -> bytes:
    def z(s: str) -> bytes:
        return s.encode("ascii") + b"\x00"
    body = b"INFO" + chunk(b"INAM", z(title)) + chunk(b"IART", z(ARTIST)) + \
        chunk(b"IPRD", z(ALBUM))
    return chunk(b"LIST", body)


def write_wav(path: str, s: Song, title: str) -> None:
    fs = s.fs
    pcm = bytearray()
    for l, r in zip(s.left, s.right):
        pcm += struct.pack("<hh", round(max(-1, min(1, l)) * PEAK * 32767),
                           round(max(-1, min(1, r)) * PEAK * 32767))
    fmt = struct.pack("<HHIIHH", 1, 2, fs, fs * 4, 4, 16)
    body = b"WAVE" + chunk(b"fmt ", fmt) + info(title) + chunk(b"data", bytes(pcm))
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def main() -> None:
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "build", "hps", "music-sd")
    os.makedirs(out, exist_ok=True)
    for i, (fs, tag) in enumerate(RATES):
        khz = f"{fs / 1000:g}"
        title = f"Prueba de sonidos {khz} kHz"
        path = os.path.join(out, f"{FIRST_NUMBER + i}_Prueba_sonidos_{tag}kHz.wav")
        s = compose(fs)
        write_wav(path, s, title)
        print(f">> {os.path.basename(path)}: {title}, {len(s.left) / fs:.1f} s")


if __name__ == "__main__":
    main()
