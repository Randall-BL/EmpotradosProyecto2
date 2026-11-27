#!/usr/bin/env python3
"""Genera los WAV de prueba del parser (DK-14, SC-03..SC-06).

Uso:  python3 tests/data/gen_wavs.py <directorio de salida>

Los archivos se generan en cada corrida (build/tests/data/) en vez de versionarse: son
deterministas y asi el repositorio no carga binarios. tests/test_wav.c conoce el
contenido de cada uno (frecuencia, canales, metadatos, error esperado).

Solo usa la biblioteca estandar (struct): corre en el PC, en el contenedor y en la CI.
"""
import math
import os
import struct
import sys


def chunk(cid: bytes, body: bytes, pad: bool = True) -> bytes:
    """Arma un chunk RIFF: id de 4 bytes + tamano (32 bits LE) + cuerpo.

    Con pad=True agrega el byte de relleno si el cuerpo tiene largo impar (lo que
    exige RIFF); con pad=False se puede generar un archivo mal formado a proposito.
    """
    data = cid + struct.pack("<I", len(body)) + body
    if pad and len(body) % 2:
        data += b"\x00"
    return data


def fmt_pcm(rate: int, ch: int, bits: int = 16, tag: int = 1) -> bytes:
    """Chunk "fmt " clasico de 16 bytes (tag 1 = PCM, 3 = float).

    Calcula block_align = canales * bits / 8 y byte_rate = rate * block_align.
    """
    align = ch * bits // 8
    return chunk(b"fmt ", struct.pack("<HHIIHH", tag, ch, rate, rate * align, align, bits))


def fmt_extensible(rate: int, ch: int, sub: int = 1) -> bytes:
    """Chunk "fmt " WAVE_FORMAT_EXTENSIBLE (0xFFFE) de 40 bytes.

    sub es el formato real que va en los 2 primeros bytes del GUID SubFormat
    (1 = PCM, 3 = float). Campos: cbSize = 22, bits validos = 16 y mascara de canales.
    """
    align = ch * 2
    guid = struct.pack("<H", sub) + b"\x00\x00\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71"
    body = struct.pack("<HHIIHHHHI", 0xFFFE, ch, rate, rate * align, align, 16, 22, 16,
                       3 if ch == 2 else 4) + guid
    return chunk(b"fmt ", body)


def tone(rate: int, ch: int, seconds: float, freq: float = 440.0) -> bytes:
    """PCM de 16 bits de un seno de freq Hz, amplitud 8000, durante seconds segundos.

    Con varios canales repite la misma muestra en cada uno (intercalado).
    """
    n = int(rate * seconds)
    out = bytearray()
    for i in range(n):
        v = int(8000 * math.sin(2 * math.pi * freq * i / rate))
        for _ in range(ch):
            out += struct.pack("<h", v)
    return bytes(out)


def info(title=None, artist=None, album=None) -> bytes:
    """Chunk LIST/INFO con INAM (titulo), IART (artista) e IPRD (album).

    Un campo None se omite. Un str se codifica en latin-1 con su byte 0 final; un
    bytes se copia tal cual (para probar textos con caracteres de control).
    """
    body = b"INFO"
    for cid, txt in ((b"INAM", title), (b"IART", artist), (b"IPRD", album)):
        if txt is not None:
            raw = txt if isinstance(txt, bytes) else txt.encode("latin-1") + b"\x00"
            body += chunk(cid, raw)
    return chunk(b"LIST", body)


def id3_frame(fid: bytes, text: str, enc: int, v4: bool) -> bytes:
    """Frame de texto ID3v2 (TIT2, TPE1, TALB...) con la codificacion enc.

    enc: 0 = latin-1, 1 = UTF-16 con BOM, 2 = UTF-16BE, 3 = UTF-8. En v2.4 el tamano
    del frame es syncsafe; en v2.3 es un entero big-endian.
    """
    if enc == 0:
        payload = b"\x00" + text.encode("latin-1")
    elif enc == 1:
        payload = b"\x01" + b"\xff\xfe" + text.encode("utf-16-le")
    elif enc == 2:
        payload = b"\x02" + text.encode("utf-16-be")
    else:
        payload = b"\x03" + text.encode("utf-8")
    size = len(payload)
    size_b = syncsafe(size) if v4 else struct.pack(">I", size)
    return fid + size_b + b"\x00\x00" + payload


def syncsafe(n: int) -> bytes:
    """Entero de 28 bits en 4 bytes de 7 bits cada uno (formato "syncsafe" de ID3v2)."""
    return bytes([(n >> 21) & 0x7F, (n >> 14) & 0x7F, (n >> 7) & 0x7F, n & 0x7F])


def id3(frames: bytes, v4: bool = False, padding: int = 10) -> bytes:
    """Chunk "id3 " con una etiqueta ID3v2.3 o 2.4: cabecera de 10 bytes + frames + relleno.
    """
    body = frames + b"\x00" * padding
    hdr = b"ID3" + bytes([4 if v4 else 3, 0, 0]) + syncsafe(len(body))
    return chunk(b"id3 ", hdr + body)


def riff(*chunks: bytes, riff_size: int = None) -> bytes:
    """Archivo completo: "RIFF" + tamano + "WAVE" + los chunks.

    riff_size permite declarar un tamano falso para probar archivos inconsistentes.
    """
    body = b"WAVE" + b"".join(chunks)
    size = len(body) if riff_size is None else riff_size
    return b"RIFF" + struct.pack("<I", size) + body


def main() -> None:
    """Genera todos los casos de prueba en la carpeta de salida (argv[1]).

    Cada entrada del diccionario f es nombre de archivo -> contenido; tests/test_wav.c y
    tests/test_producer.c conocen lo que se espera de cada uno.
    """
    out = sys.argv[1] if len(sys.argv) > 1 else "build/tests/data"
    os.makedirs(out, exist_ok=True)
    f = {}

    # --- validos: las cuatro frecuencias, mono y estereo ---
    f["pcm16_8k_mono.wav"] = riff(fmt_pcm(8000, 1), info("Tono 8k", "Prueba", "Parser"),
                                  chunk(b"data", tone(8000, 1, 2.5)))
    f["pcm16_16k_stereo.wav"] = riff(fmt_pcm(16000, 2), chunk(b"data", tone(16000, 2, 3.0)),
                                     info("Tono 16k", "Prueba", None))
    f["pcm16_44k_stereo.wav"] = riff(fmt_pcm(44100, 2), info("Tono 44.1k", "Artista", "Album"),
                                     chunk(b"data", tone(44100, 2, 1.0)))
    f["pcm16_48k_mono.wav"] = riff(fmt_pcm(48000, 1), chunk(b"data", tone(48000, 1, 0.5)))

    # --- chunks extra con tamano impar y relleno, LIST despues de data ---
    f["extra_chunks.wav"] = riff(chunk(b"JUNK", b"\x01" * 7), fmt_pcm(8000, 2),
                                 chunk(b"fact", struct.pack("<I", 8000)),
                                 chunk(b"bext", b"x" * 13), chunk(b"data", tone(8000, 2, 1.0)),
                                 chunk(b"cue ", b"\x00" * 5), info("Extra", "Chunks", "Impares"))
    # data con tamano impar (un byte de mas) seguido de relleno y LIST
    odd = tone(8000, 1, 0.25) + b"\x7f"
    f["odd_data.wav"] = riff(fmt_pcm(8000, 1), chunk(b"data", odd), info("Impar", "A", "B"))

    # --- metadatos ---
    long_title = "Un titulo larguisimo que no cabe en el campo de cuarenta y ocho bytes"
    f["meta_long.wav"] = riff(fmt_pcm(16000, 1),
                              info(long_title, b"Art\x01ista con control\x00", "Album  "),
                              chunk(b"data", tone(16000, 1, 0.1)))
    f["meta_empty_info.wav"] = riff(fmt_pcm(16000, 1), chunk(b"LIST", b"INFO"),
                                    chunk(b"data", tone(16000, 1, 0.1)))
    f["id3_v23.wav"] = riff(fmt_pcm(44100, 2), chunk(b"data", tone(44100, 2, 0.1)),
                            id3(id3_frame(b"TIT2", "Titulo ID3", 0, False) +
                                id3_frame(b"TPE1", "Artista UTF16", 1, False) +
                                id3_frame(b"TALB", "Album BE", 2, False)))
    f["id3_v24_and_info.wav"] = riff(fmt_pcm(44100, 2), info("Titulo INFO", None, None),
                                     chunk(b"data", tone(44100, 2, 0.1)),
                                     id3(id3_frame(b"TIT2", "Titulo ID3", 3, True) +
                                         id3_frame(b"TPE1", "Artista ID3", 3, True), v4=True))
    f["no_meta.wav"] = riff(fmt_pcm(8000, 1), chunk(b"data", tone(8000, 1, 0.1)))
    f["extensible.wav"] = riff(fmt_extensible(48000, 2), chunk(b"data", tone(48000, 2, 0.2)))

    # --- truncados ---
    full = riff(fmt_pcm(16000, 2), chunk(b"data", tone(16000, 2, 1.0)))
    f["trunc_data.wav"] = full[: len(full) // 2 + 1]          # data declara mas de lo que hay
    f["trunc_header.wav"] = full[:30]                          # se corta dentro de fmt
    f["trunc_tiny.wav"] = full[:10]
    f["no_data.wav"] = riff(fmt_pcm(8000, 1), info("Sin datos", None, None))

    # --- rechazo: formato no soportado ---
    f["pcm8.wav"] = riff(fmt_pcm(8000, 1, bits=8), chunk(b"data", b"\x80" * 800))
    f["pcm24.wav"] = riff(fmt_pcm(44100, 2, bits=24), chunk(b"data", b"\x00" * 600))
    f["float32.wav"] = riff(fmt_pcm(44100, 2, bits=32, tag=3), chunk(b"data", b"\x00" * 800))
    f["three_ch.wav"] = riff(fmt_pcm(16000, 3), chunk(b"data", b"\x00" * 600))
    f["rate_22k.wav"] = riff(fmt_pcm(22050, 2), chunk(b"data", b"\x00" * 400))
    f["ext_float.wav"] = riff(fmt_extensible(48000, 2, sub=3), chunk(b"data", b"\x00" * 400))

    # --- rechazo: no es WAV o esta mal formado ---
    f["not_riff.wav"] = b"RIFX" + full[4:]
    f["not_wave.wav"] = full[:8] + b"AVI " + full[12:]
    bad_align = bytearray(riff(fmt_pcm(8000, 2), chunk(b"data", tone(8000, 2, 0.1))))
    struct.pack_into("<H", bad_align, 12 + 8 + 12, 3)          # block_align incoherente
    f["bad_align.wav"] = bytes(bad_align)
    f["fmt_short.wav"] = riff(chunk(b"fmt ", b"\x01\x00\x01\x00"), chunk(b"data", b"\x00" * 4))

    for name, data in sorted(f.items()):
        with open(os.path.join(out, name), "wb") as fh:
            fh.write(data)
    print(f"gen_wavs: {len(f)} archivos en {out}")


if __name__ == "__main__":
    main()
