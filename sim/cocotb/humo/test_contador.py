"""Prueba trivial (DK-09): el contador arranca en 0, cuenta con en = 1, se detiene con
en = 0 y da la vuelta en 255 -> 0."""
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, FallingEdge, RisingEdge


@cocotb.test()
async def cuenta_y_da_la_vuelta(dut):
    cocotb.start_soon(Clock(dut.clk, 20, units="ns").start())
    dut.reset.value = 1
    dut.en.value = 0
    await ClockCycles(dut.clk, 2)
    dut.reset.value = 0
    await FallingEdge(dut.clk)
    assert int(dut.q.value) == 0, "tras el reset el contador debe valer 0"

    dut.en.value = 1
    await ClockCycles(dut.clk, 10)
    await FallingEdge(dut.clk)
    assert int(dut.q.value) == 10, f"tras 10 ciclos: {int(dut.q.value)}"

    dut.en.value = 0
    await ClockCycles(dut.clk, 5)
    await FallingEdge(dut.clk)
    assert int(dut.q.value) == 10, "con en = 0 no debe contar"

    dut.en.value = 1
    await ClockCycles(dut.clk, 246)
    await FallingEdge(dut.clk)
    assert int(dut.q.value) == 0, f"255 + 1 debe dar la vuelta a 0: {int(dut.q.value)}"
