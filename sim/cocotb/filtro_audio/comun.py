"""Utilidades compartidas por los testbenches cocotb de filtro_audio."""
import os
import pathlib

from avalon import AvalonMaster, start

ROOT = pathlib.Path(os.environ.get("REPO_ROOT", pathlib.Path(__file__).resolve().parents[3]))

# Direcciones de los registros (en palabras), como en hw/rtl/filtro_audio.v.
CTRL, STATUS, NTAPS, GAIN, CADDR, CDATA, DIN, DOUT = range(8)
ST_READY, ST_BUSY, ST_SAT = 1, 2, 4


def s16(v):
    """16 bits sin signo -> entero con signo."""
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def pack(l, r):
    """Par estereo como lo ve el bus: {R[31:16], L[15:0]}."""
    return ((r & 0xFFFF) << 16) | (l & 0xFFFF)


def unpack(w):
    return s16(w), s16(w >> 16)


async def nuevo(dut):
    """Reset y espera a que el nucleo termine de borrar la linea de retardo."""
    await start(dut)
    av = AvalonMaster(dut)
    await esperar_libre(av)
    return av


async def esperar_libre(av):
    while await av.rd(STATUS) & ST_BUSY:
        pass


def coeficientes_hex():
    """Los 256 coeficientes de hw/rtl/fir_coefs.hex (2 bancos x 128) con signo."""
    lines = (ROOT / "hw/rtl/fir_coefs.hex").read_text().split()
    return [s16(int(x, 16)) for x in lines]


async def cargar_coeficientes(av, coefs):
    """Escribe los 256 coeficientes por COEF_ADDR/COEF_DATA (autoincremento por banco)."""
    await av.write(CADDR, 0)
    for i, c in enumerate(coefs):
        if i == 128:
            await av.write(CADDR, 0x80)
        await av.write(CDATA, c & 0xFFFF)
