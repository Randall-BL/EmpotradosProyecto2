#include "clock.h"

static const uint32_t STEP_MS[4] = { 1000u, 100u, 10u, 1u };

void clk_init(clk_t *c, uint32_t ticks_per_ms, uint32_t now_ticks)
{
    c->last = now_ticks;
    c->frac = 0;
    c->ms = 0;
    /* Unica multiplicacion: al configurar (en RV32I es una llamada a __mulsi3). */
    for (uint32_t i = 0; i < 4u; ++i)
        c->per_ms[i] = ticks_per_ms * STEP_MS[i];
}

uint32_t clk_update(clk_t *c, uint32_t now_ticks)
{
    uint32_t delta = now_ticks - c->last;     /* modular: sirve aunque el contador de vuelta */
    c->last = now_ticks;
    /* frac < per_ms[3] antes de sumar, asi que no desborda si delta < 2^32 - per_ms. */
    c->frac += delta;
    for (uint32_t i = 0; i < 4u; ++i) {
        while (c->frac >= c->per_ms[i]) {
            c->frac -= c->per_ms[i];
            c->ms += STEP_MS[i];
        }
    }
    return c->ms;
}
