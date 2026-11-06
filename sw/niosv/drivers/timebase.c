#include "timebase.h"
#include "clock.h"
#include "hw.h"
#include "system.h"

static clk_t clk;

uint32_t timebase_cycles(void)
{
    return HW_REG(ALT_CPU_MTIME_OFFSET, 0);      /* mtime[31:0] */
}

void timebase_init(uint32_t ticks_per_ms)
{
    if (ticks_per_ms == 0)
        ticks_per_ms = ALT_CPU_CPU_FREQ / 1000u; /* constante: no divide en ejecucion */
    clk_init(&clk, ticks_per_ms, timebase_cycles());
}

uint32_t timebase_ms(void)
{
    return clk_update(&clk, timebase_cycles());
}
