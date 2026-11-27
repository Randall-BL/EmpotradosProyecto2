/*
 * Reloj en milisegundos sin division (NV-12). Ver clock.h.
 *
 * Idea: en vez de calcular ms = ciclos / ticks_per_ms (una division de 64 bits que el
 * Nios V/m tendria que emular en software), se acumulan los ciclos transcurridos en
 * frac y se van "cobrando" en monedas de 1000, 100, 10 y 1 ms. Como frac siempre queda
 * por debajo de 1 ms de ciclos, cada llamada hace pocas restas.
 */
#include "clock.h"

/* Tamanos de las "monedas" en ms, de mayor a menor. */
static const uint32_t STEP_MS[4] = { 1000u, 100u, 10u, 1u };

/*
 * Prepara el acumulador.
 *   ticks_per_ms: ciclos del contador que equivalen a 1 ms (50 000 a 50 MHz).
 *   now_ticks:    lectura actual del contador; se toma como el instante 0 ms.
 * Precalcula cuantos ciclos tiene cada "moneda" (1000, 100, 10 y 1 ms).
 */
void clk_init(clk_t *c, uint32_t ticks_per_ms, uint32_t now_ticks)
{
    c->last = now_ticks;
    c->frac = 0;
    c->ms = 0;
    /* Unica multiplicacion: al configurar (en RV32I es una llamada a __mulsi3). */
    for (uint32_t i = 0; i < 4u; ++i)
        c->per_ms[i] = ticks_per_ms * STEP_MS[i];
}

/*
 * Suma los ciclos que pasaron desde la ultima llamada y devuelve los ms acumulados
 * desde clk_init(). Debe llamarse al menos una vez cada 2^32 ciclos para no perder
 * vueltas del contador.
 */
uint32_t clk_update(clk_t *c, uint32_t now_ticks)
{
    uint32_t delta = now_ticks - c->last;     /* modular: sirve aunque el contador de vuelta */
    c->last = now_ticks;
    /* frac < per_ms[3] antes de sumar, asi que no desborda si delta < 2^32 - per_ms. */
    c->frac += delta;
    /* Se convierte frac en ms restando primero de a 1 s, luego 100 ms, 10 ms y 1 ms:
     * como mucho 9 restas por escalon (salvo el de 1 s si paso mucho tiempo). */
    for (uint32_t i = 0; i < 4u; ++i) {
        while (c->frac >= c->per_ms[i]) {
            c->frac -= c->per_ms[i];
            c->ms += STEP_MS[i];
        }
    }
    return c->ms;
}
