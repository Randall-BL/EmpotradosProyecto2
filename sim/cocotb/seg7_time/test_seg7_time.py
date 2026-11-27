"""seg7_time aislado, solo por Avalon-MM (DK-13).

Escribe tiempos y numeros de cancion, espera la conversion (STATUS.BUSY) y decodifica
los segmentos de vuelta a digitos. Mismos casos que sim/questa/rtl/tb_seg7_time.sv, mas
el valor de los registros y el BCD de STATUS.
"""
import cocotb
from cocotb.triggers import FallingEdge, RisingEdge

from avalon import AvalonMaster, start

CTRL, TIME, TRACK, STATUS = range(4)
DIGITO = {0b1000000: "0", 0b1111001: "1", 0b0100100: "2", 0b0110000: "3",
          0b0011001: "4", 0b0010010: "5", 0b0000010: "6", 0b1111000: "7",
          0b0000000: "8", 0b0010000: "9", 0b1111111: " "}


def mostrado(dut):
    """Lo que muestran HEX5..HEX0 como "PP MM:SS"."""
    d = [DIGITO.get(int(getattr(dut, f"hex{i}").value), "?") for i in range(6)]
    return f"{d[5]}{d[4]} {d[3]}{d[2]}:{d[1]}{d[0]}"


async def poner_tiempo(av, segundos):
    await av.write(TIME, segundos)
    while (await av.rd(STATUS)) >> 16 & 1:      # BUSY mientras convierte
        pass


@cocotb.test()
async def tiempos_y_limites(dut):
    await start(dut, cycles=3)
    av = AvalonMaster(dut)
    await FallingEdge(dut.clk)
    assert mostrado(dut) == "   00:00", f"reset: '{mostrado(dut)}'"
    casos = [(0, "00:00"), (59, "00:59"), (125, "02:05"), (599, "09:59"),
             (3599, "59:59"), (5999, "99:59"), (6000, "99:59"), (8191, "99:59")]
    for segundos, esperado in casos:
        await poner_tiempo(av, segundos)
        assert mostrado(dut) == f"   {esperado}", f"TIME = {segundos}: '{mostrado(dut)}'"
        dut._log.info("TIME = %4d s -> '%s'", segundos, mostrado(dut))
    st = await av.rd(STATUS)
    assert st >> 24 == 0x01 and st & 0xFFFF == 0x9959, f"STATUS = 0x{st:08x}"


@cocotb.test()
async def cancion_y_apagado(dut):
    await start(dut, cycles=3)
    av = AvalonMaster(dut)
    await poner_tiempo(av, 125)
    pasos = [(TRACK, 7, None), (CTRL, 3, "07 02:05"), (TRACK, 150, "99 02:05"),
             (CTRL, 0, "     :  "), (CTRL, 1, "   02:05")]
    for reg, val, esperado in pasos:
        await av.write(reg, val)
        if esperado is None:
            continue
        await RisingEdge(dut.clk)
        await FallingEdge(dut.clk)
        assert mostrado(dut) == esperado, f"reg {reg} = {val}: '{mostrado(dut)}', se esperaba '{esperado}'"
        dut._log.info("reg %d = %3d -> '%s'", reg, val, mostrado(dut))
    assert await av.rd(TRACK) == 99, "TRACK se guarda saturado a 99"
    assert await av.rd(TIME) == 125
    assert await av.rd(CTRL) == 1
