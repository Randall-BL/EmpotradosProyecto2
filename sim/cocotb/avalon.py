"""Maestro Avalon-MM para los testbenches cocotb del HDL propio.

Hace lo mismo que las tareas avs_wr/avs_rd de los testbenches de Questa
(sim/questa/rtl/): pone la direccion y el dato en un flanco de subida y mantiene el
pedido mientras el esclavo levante waitrequest. Las senales se muestrean justo en el
flanco, antes de que el DUT actualice sus registros, que es como las ve el bus.
"""
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import FallingEdge, RisingEdge


async def start(dut, cycles=5):
    """Arranca el reloj de 50 MHz y aplica el reset sincrono durante `cycles` ciclos."""
    cocotb.start_soon(Clock(dut.clk, 20, units="ns").start())
    dut.reset.value = 1
    dut.avs_read.value = 0
    dut.avs_write.value = 0
    dut.avs_address.value = 0
    dut.avs_writedata.value = 0
    for _ in range(cycles):
        await RisingEdge(dut.clk)
    dut.reset.value = 0


class AvalonMaster:
    """Maestro de un esclavo con prefijo avs_. Sin waitrequest, la lectura tiene
    latencia fija de 1 ciclo (como seg7_time)."""

    def __init__(self, dut):
        self.dut = dut
        self.clk = dut.clk
        self.has_wait = hasattr(dut, "avs_waitrequest")

    async def _until_accepted(self):
        """Espera el flanco en que waitrequest esta en 0 (transferencia aceptada).
        Devuelve los ciclos que duro el pedido."""
        cycles = 0
        while True:
            await RisingEdge(self.clk)
            cycles += 1
            if not self.has_wait or not self.dut.avs_waitrequest.value:
                return cycles

    async def write(self, addr, data):
        await RisingEdge(self.clk)
        self.dut.avs_address.value = addr
        self.dut.avs_writedata.value = data & 0xFFFFFFFF
        self.dut.avs_write.value = 1
        await self._until_accepted()
        self.dut.avs_write.value = 0

    async def read(self, addr):
        """Devuelve (dato, ciclos que tardo la lectura)."""
        await RisingEdge(self.clk)
        self.dut.avs_address.value = addr
        self.dut.avs_read.value = 1
        if self.has_wait:
            cycles = await self._until_accepted()
            data = int(self.dut.avs_readdata.value)
            self.dut.avs_read.value = 0
            return data, cycles
        await RisingEdge(self.clk)               # latencia fija de 1 ciclo
        self.dut.avs_read.value = 0
        await FallingEdge(self.clk)
        return int(self.dut.avs_readdata.value), 1

    async def rd(self, addr):
        """Solo el dato leido."""
        return (await self.read(addr))[0]
