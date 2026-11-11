/*
 * Acumulador de milisegundos a partir de un contador de ciclos (NV-12).
 *
 * El Nios V/m no tiene divisor: convertir mtime (50 MHz) a ms con una division costaria
 * una llamada a __udivdi3 en cada lectura. En cambio se acumula la diferencia de
 * ciclos desde la lectura anterior y se resta de a 1000, 100, 10 y 1 ms. Basta con
 * llamar clk_update() al menos una vez cada 2^32 ciclos (85 s a 50 MHz); el lazo
 * principal lo hace en cada vuelta.
 */
#ifndef COMMON_CLOCK_H
#define COMMON_CLOCK_H

#include <stdint.h>

typedef struct {
    uint32_t last;                        /* ultima lectura del contador */
    uint32_t frac;                        /* ciclos que todavia no completan 1 ms */
    uint32_t ms;                          /* milisegundos desde clk_init */
    uint32_t per_ms[4];                   /* ciclos de 1000, 100, 10 y 1 ms */
} clk_t;

/* ticks_per_ms: ciclos del contador por milisegundo (50 000 a 50 MHz). */
void clk_init(clk_t *c, uint32_t ticks_per_ms, uint32_t now_ticks);

/* Actualiza con la lectura actual del contador y devuelve los ms acumulados. */
uint32_t clk_update(clk_t *c, uint32_t now_ticks);

#endif /* COMMON_CLOCK_H */
