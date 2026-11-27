"""Interfaz de registros Avalon-MM de filtro_audio, aislado (DK-12).

Valores de reset, lectura/escritura de cada registro, limites de NTAPS, bits de solo
lectura, carga y relectura de coeficientes con autoincremento y vuelta dentro del banco,
CLEAR, escritura en un registro de solo lectura y espera (waitrequest) mientras el filtro
trabaja. Equivale a la parte de registros de sim/questa/rtl/tb_filtro_audio.sv.
"""
import cocotb
from cocotb.triggers import RisingEdge

from avalon import AvalonMaster, start
from comun import (CADDR, CDATA, CTRL, DIN, DOUT, GAIN, NTAPS, STATUS, ST_BUSY,
                   esperar_libre, nuevo)


@cocotb.test()
async def valores_de_reset(dut):
    """Tras el reset: borra la linea de retardo (BUSY) y luego queda en bypass, 127 taps,
    ganancia 1.0 y version 1.0 con 128 taps maximos."""
    await start(dut)
    av = AvalonMaster(dut)
    assert await av.rd(STATUS) == 0x0100_8002, "justo tras el reset debe estar BUSY"
    await esperar_libre(av)
    esperado = {CTRL: 0x1, STATUS: 0x0100_8000, NTAPS: 127, GAIN: 0x8000, CADDR: 0,
                DIN: 0, DOUT: 0}
    for reg, val in esperado.items():
        got = await av.rd(reg)
        assert got == val, f"registro {reg}: 0x{got:08x}, se esperaba 0x{val:08x}"


@cocotb.test()
async def lectura_escritura_y_limites(dut):
    av = await nuevo(dut)
    await av.write(CTRL, 0x2)
    assert await av.rd(CTRL) == 0x2, "CTRL: banco 1 sin bypass"
    await av.write(CTRL, 0x7)
    assert await av.rd(CTRL) == 0x3, "CTRL.CLEAR se lee 0"
    for escrito, leido in [(0, 1), (200, 128), (64, 64), (128, 128), (1, 1)]:
        await av.write(NTAPS, escrito)
        got = await av.rd(NTAPS)
        assert got == leido, f"NTAPS {escrito} -> {got}, se esperaba {leido}"
    await av.write(GAIN, 0xFFFF_ABCD)
    assert await av.rd(GAIN) == 0xABCD, "GAIN guarda solo 16 bits"
    await av.write(DIN, 0x1234_5678)
    assert await av.rd(DIN) == 0x1234_5678, "DATA_IN devuelve la ultima entrada"


@cocotb.test()
async def bits_de_solo_lectura(dut):
    """STATUS solo cambia por W1C de SAT; DATA_OUT ignora las escrituras."""
    av = await nuevo(dut)
    await av.write(STATUS, 0xFFFF_FFFF)
    assert await av.rd(STATUS) == 0x0100_8000, "STATUS es de solo lectura"
    antes = await av.rd(DOUT)
    await av.write(DOUT, 0xDEAD_BEEF)
    assert await av.rd(DOUT) == antes, "DATA_OUT es de solo lectura"


@cocotb.test()
async def coeficientes_autoincremento(dut):
    """COEF_DATA guarda y avanza COEF_ADDR dentro del banco (127 -> 0) y se relee con
    signo extendido."""
    av = await nuevo(dut)
    await av.write(CADDR, 0xFE)                  # banco 1, indice 126
    for c in (0x1234, 0x8001, 0x7FFF):
        await av.write(CDATA, c)
    assert await av.rd(CADDR) == 0x81, "autoincremento con vuelta dentro del banco 1"
    for addr, val in [(0xFE, 0x0000_1234), (0xFF, 0xFFFF_8001), (0x80, 0x0000_7FFF)]:
        await av.write(CADDR, addr)
        got = await av.rd(CDATA)
        assert got == val, f"coef[0x{addr:02x}] = 0x{got:08x}, se esperaba 0x{val:08x}"
    await av.write(CADDR, 0x05)                  # el banco 0 no se toco
    assert await av.rd(CDATA) == 0, "escribir el banco 1 no cambia el banco 0"


@cocotb.test()
async def clear_y_espera(dut):
    """CLEAR deja BUSY 128 ciclos; la lectura de DATA_OUT espera (waitrequest) hasta que
    termina el calculo, unos NTAPS + 9 ciclos."""
    av = await nuevo(dut)
    await av.write(CTRL, 0x4)                    # CLEAR, banco 0, sin bypass
    assert await av.rd(STATUS) & ST_BUSY, "CLEAR debe poner BUSY"
    await esperar_libre(av)
    await av.write(DIN, 0x0001_0001)
    _, ciclos = await av.read(DOUT)
    assert 127 <= ciclos <= 140, f"la lectura de DATA_OUT tardo {ciclos} ciclos"
    dut._log.info("Lectura de DATA_OUT tras DATA_IN: %d ciclos con 127 taps", ciclos)
    await RisingEdge(dut.clk)
    assert not (await av.rd(STATUS) & ST_BUSY), "termina libre"
