/* Base de tiempo del Nios V a partir de mtime (NV-12). Ver timebase.h. */
#include "timebase.h"
#include "clock.h"
#include "hw.h"
#include "system.h"

/* Acumulador de ms (clock.h): convierte ciclos a ms sin dividir. */
static clk_t clk;

/* Palabra baja de mtime: ciclos de CPU, da la vuelta cada 2^32 (~85 s a 50 MHz). */
uint32_t timebase_cycles(void)
{
    return HW_REG(ALT_CPU_MTIME_OFFSET, 0);      /* mtime[31:0] */
}

/* Fija el instante 0 ms y los ciclos por ms (0 = frecuencia real del CPU). */
void timebase_init(uint32_t ticks_per_ms)
{
    if (ticks_per_ms == 0)
        ticks_per_ms = ALT_CPU_CPU_FREQ / 1000u; /* constante: no divide en ejecucion */
    clk_init(&clk, ticks_per_ms, timebase_cycles());
}

/* Milisegundos desde timebase_init(). */
uint32_t timebase_ms(void)
{
    return clk_update(&clk, timebase_cycles());
}
