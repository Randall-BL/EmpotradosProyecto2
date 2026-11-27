"""Datapath de filtro_audio contra el modelo NumPy/SciPy (DK-11).

La referencia es FirModel de scripts/design_fir.py: el modelo bit a bit del datapath con
los mismos coeficientes cuantizados (Q1.15) que se cargan en el modulo. Como el modelo
redondea y satura exactamente como el hardware (CD-14), la tolerancia contra el modelo
es 0 LSB. Contra la matematica ideal la tolerancia es 1 LSB: se redondea una sola vez
por etapa (error <= 0.5 LSB) y la entrada a fondo de escala es 32767/32768, no 1.

Casos: coeficientes iguales al diseno, reset y bypass (identidad), impulso, escalon,
senos en banda de paso y de rechazo de los dos bancos, saturacion con entradas extremas
y los 8 vectores dorados de sim/vectors/.
"""
import math

import cocotb
import numpy as np

import design_fir as fir
from comun import (CADDR, CDATA, CTRL, DIN, DOUT, GAIN, NTAPS, ROOT, STATUS, ST_SAT, cargar_coeficientes,
                   coeficientes_hex, esperar_libre, nuevo, pack, unpack)

COEFS = coeficientes_hex()
BANCO = [COEFS[:fir.NTAPS], COEFS[128:128 + fir.NTAPS]]


async def preparar(dut):
    av = await nuevo(dut)
    await cargar_coeficientes(av, COEFS)
    return av


async def procesar(av, xs_l, xs_r, banco=0, ntaps=127, ganancia=0x8000, bypass=0):
    """Configura el modulo (con CLEAR y SAT borrado), pasa cada par por DATA_IN/DATA_OUT
    y devuelve (salidas L, salidas R, STATUS.SAT, peor latencia en ciclos)."""
    await av.write(NTAPS, ntaps)
    await av.write(GAIN, ganancia)
    await av.write(CTRL, 0x4 | (banco << 1) | bypass)
    await av.write(STATUS, ST_SAT)
    await esperar_libre(av)
    ys_l, ys_r, peor = [], [], 0
    for l, r in zip(xs_l, xs_r):
        await av.write(DIN, pack(int(l), int(r)))
        w, ciclos = await av.read(DOUT)
        peor = max(peor, ciclos)
        yl, yr = unpack(w)
        ys_l.append(yl)
        ys_r.append(yr)
    sat = bool(await av.rd(STATUS) & ST_SAT)
    return ys_l, ys_r, sat, peor


def modelo(xs_l, xs_r, banco=0, ntaps=127, ganancia=0x8000, bypass=0):
    """Salida esperada de FirModel (un modelo por canal) y si satura."""
    ml = fir.FirModel(BANCO[banco], ntaps)
    mr = fir.FirModel(BANCO[banco], ntaps)
    yl = [ml.step(int(x), bool(bypass), ganancia) for x in xs_l]
    yr = [mr.step(int(x), bool(bypass), ganancia) for x in xs_r]
    return yl, yr, ml.sat or mr.sat


def igual_al_modelo(nombre, got_l, got_r, exp_l, exp_r):
    for n, (gl, gr, el, er) in enumerate(zip(got_l, got_r, exp_l, exp_r)):
        assert (gl, gr) == (el, er), (
            f"{nombre}, muestra {n}: salida ({gl}, {gr}), el modelo da ({el}, {er})")


def seno(f, n, amp=0.5):
    k = np.arange(n)
    return np.round(amp * 32767 * np.sin(2 * math.pi * f * k / fir.FS)).astype(int)


def ganancia_db(x, y, desde):
    """Relacion RMS salida/entrada en dB, despues del transitorio."""
    rx = np.sqrt(np.mean(np.square(np.asarray(x[desde:], float))))
    ry = np.sqrt(np.mean(np.square(np.asarray(y[desde:], float))))
    return -math.inf if ry == 0 else 20 * math.log10(ry / rx)


@cocotb.test()
async def coeficientes_iguales_al_diseno(dut):
    """fir_coefs.hex es la cuantizacion Q1.15 del diseno de SciPy, y el modulo devuelve
    lo que se le cargo."""
    disenados = [fir.quantize(h) for h in fir.design().values()]
    for b in (0, 1):
        assert list(map(int, disenados[b])) == BANCO[b], f"banco {b} distinto del diseno"
        assert COEFS[128 * b + fir.NTAPS:128 * (b + 1)] == [0], "relleno del banco"
    av = await preparar(dut)
    for addr in (0, 63, 126, 0x80, 0x80 + 63, 0x80 + 126):
        await av.write(CADDR, addr)
        got = await av.rd(CDATA)
        exp = COEFS[(addr & 0x7F) + (128 if addr & 0x80 else 0)] & 0xFFFFFFFF
        assert got == exp, f"coef[0x{addr:02x}] = 0x{got:08x}, se esperaba 0x{exp:08x}"


@cocotb.test()
async def reset_y_bypass_identidad(dut):
    """Tras el reset el modulo esta en bypass con ganancia 1.0: la salida es la entrada."""
    av = await nuevo(dut)                        # sin tocar CTRL ni GAIN
    rng = np.random.default_rng(1)
    xs = rng.integers(-32768, 32768, size=(2, 64))
    xs[0][:4] = [32767, -32768, 0, -1]
    ys = []
    for l, r in zip(*xs):
        await av.write(DIN, pack(int(l), int(r)))
        ys.append(unpack(await av.rd(DOUT)))
    assert ys == [(int(l), int(r)) for l, r in zip(*xs)], "en bypass con ganancia 1.0 la salida debe ser la entrada"
    assert not (await av.rd(STATUS) & ST_SAT), "la identidad no satura"


@cocotb.test()
async def impulso(dut):
    """Impulso a fondo de escala en L: la salida son los coeficientes (+-1 LSB) y despues
    de 127 muestras vuelve a 0; R queda en 0."""
    av = await preparar(dut)
    n = 140
    xl, xr = [32767] + [0] * (n - 1), [0] * n
    yl, yr, sat, peor = await procesar(av, xl, xr)
    el, er, esat = modelo(xl, xr)
    igual_al_modelo("impulso", yl, yr, el, er)
    for k, h in enumerate(BANCO[0]):
        assert abs(yl[k] - h) <= 1, f"impulso: y[{k}] = {yl[k]}, h[{k}] = {h}"
    assert yl[127:] == [0] * (n - 127) and yr == [0] * n
    assert sat == esat == False
    dut._log.info("impulso: 140 muestras iguales al modelo; latencia maxima %d ciclos", peor)


@cocotb.test()
async def escalon(dut):
    """Escalon de 0.5: el valor final es 0.5 * suma de coeficientes (+-1 LSB); R, con el
    escalon negativo, da el valor opuesto."""
    av = await preparar(dut)
    n = 200
    xl, xr = [16384] * n, [-16384] * n
    yl, yr, sat, _ = await procesar(av, xl, xr)
    el, er, _ = modelo(xl, xr)
    igual_al_modelo("escalon", yl, yr, el, er)
    assert abs(yl[-1] - 16384 * sum(BANCO[0]) / 32768) <= 1, f"valor final {yl[-1]}"
    assert abs(yl[-1] + yr[-1]) <= 1, "R debe ser el opuesto de L"
    assert not sat
    dut._log.info("escalon: valor final %d (0.5 x suma de h = %.1f)", yl[-1], 0.5 * sum(BANCO[0]))


@cocotb.test()
async def senos_pasa_bajos(dut):
    """Banco 0 (pasa-bajos 1 kHz): 500 Hz pasa (|error| < 0.5 dB) y 8 kHz se rechaza
    (>= 60 dB; el diseno da 67.8 dB por encima de 2.4 kHz)."""
    av = await preparar(dut)
    n = 400
    xl, xr = seno(500, n), seno(8000, n)
    yl, yr, sat, _ = await procesar(av, xl, xr)
    el, er, _ = modelo(xl, xr)
    igual_al_modelo("senos banco 0", yl, yr, el, er)
    g_paso, g_rechazo = ganancia_db(xl, yl, 160), ganancia_db(xr, yr, 160)
    dut._log.info("banco 0: 500 Hz %.2f dB, 8 kHz %.1f dB", g_paso, g_rechazo)
    assert abs(g_paso) < 0.5, f"500 Hz deberia pasar: {g_paso:.2f} dB"
    assert g_rechazo <= -60, f"8 kHz deberia rechazarse: {g_rechazo:.1f} dB"
    assert not sat


@cocotb.test()
async def senos_pasa_banda(dut):
    """Banco 1 (pasa-banda 1-4 kHz): 2 kHz pasa y 10 kHz se rechaza (>= 55 dB; el diseno
    da 61.4 dB por encima de 5.6 kHz)."""
    av = await preparar(dut)
    n = 400
    xl, xr = seno(2000, n), seno(10000, n)
    yl, yr, sat, _ = await procesar(av, xl, xr, banco=1)
    el, er, _ = modelo(xl, xr, banco=1)
    igual_al_modelo("senos banco 1", yl, yr, el, er)
    g_paso, g_rechazo = ganancia_db(xl, yl, 160), ganancia_db(xr, yr, 160)
    dut._log.info("banco 1: 2 kHz %.2f dB, 10 kHz %.1f dB", g_paso, g_rechazo)
    assert abs(g_paso) < 0.5, f"2 kHz deberia pasar: {g_paso:.2f} dB"
    assert g_rechazo <= -55, f"10 kHz deberia rechazarse: {g_rechazo:.1f} dB"
    assert not sat


@cocotb.test()
async def saturacion(dut):
    """Entradas extremas con el signo de cada coeficiente: la suma pasa de 16 bits, la
    salida se satura a 32767 / -32768 igual que el modelo y STATUS.SAT queda en 1 hasta
    que se borra con W1C."""
    av = await preparar(dut)
    h = BANCO[0]
    patron = [32767 if c >= 0 else -32768 for c in reversed(h)]
    xl = patron + patron[:73]
    xr = [-x if x != -32768 else 32767 for x in xl]
    yl, yr, sat, _ = await procesar(av, xl, xr)
    el, er, esat = modelo(xl, xr)
    igual_al_modelo("saturacion", yl, yr, el, er)
    assert esat and sat, "el modelo y el hardware deben marcar saturacion"
    assert 32767 in yl and -32768 in yr, "la salida debe llegar a los limites"
    await av.write(STATUS, ST_SAT)
    assert not (await av.rd(STATUS) & ST_SAT), "SAT se borra escribiendo 1"


@cocotb.test()
async def vectores_dorados(dut):
    """Los 8 casos de sim/vectors/ (los mismos de tb_filtro_audio en Questa), bit a bit."""
    av = await preparar(dut)
    satura = {"saturacion", "ganancia_alta"}
    total = 0
    for nombre in ["impulso", "escalon", "senos", "saturacion", "ruido_banco1",
                   "bypass_gain", "taps16", "ganancia_alta"]:
        lineas = (ROOT / f"sim/vectors/fir_{nombre}.txt").read_text().split("\n")
        banco, ntaps, ganancia, bypass, n = map(int, lineas[0].split())
        pares = [tuple(int(v, 16) for v in ln.split()) for ln in lineas[1:1 + n]]
        xs = [unpack(i) for i, _ in pares]
        yl, yr, sat, _ = await procesar(av, [x[0] for x in xs], [x[1] for x in xs],
                                        banco, ntaps, ganancia, bypass)
        for k, ((_, o), gl, gr) in enumerate(zip(pares, yl, yr)):
            assert pack(gl, gr) == o, f"fir_{nombre} muestra {k}: 0x{pack(gl, gr):08x}, se esperaba 0x{o:08x}"
        assert sat == (nombre in satura), f"fir_{nombre}: SAT = {sat}"
        total += n
        dut._log.info("fir_%-13s %3d muestras iguales, SAT = %d", nombre, n, sat)
    dut._log.info("vectores dorados: %d muestras, 0 diferencias", total)
