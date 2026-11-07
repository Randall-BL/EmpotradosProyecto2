#include "seg7.h"
#include "hw.h"

#define S7_CTRL   0x0
#define S7_TIME   0x4
#define S7_TRACK  0x8
#define S7_STATUS 0xC

#define CTRL_ON    (1u << 0)
#define CTRL_TRACK (1u << 1)

static uint32_t ctrl_shadow = CTRL_ON;

void seg7_init(uintptr_t base)
{
    ctrl_shadow = CTRL_ON;
    HW_REG(base, S7_CTRL) = ctrl_shadow;
    seg7_set_time(base, 0);
}

void seg7_set_time(uintptr_t base, uint32_t seconds)
{
    HW_REG(base, S7_TIME) = seconds;
}

void seg7_set_track(uintptr_t base, uint32_t track, int show)
{
    HW_REG(base, S7_TRACK) = track;
    ctrl_shadow = show ? (ctrl_shadow | CTRL_TRACK) : (ctrl_shadow & ~CTRL_TRACK);
    HW_REG(base, S7_CTRL) = ctrl_shadow;
}

void seg7_enable(uintptr_t base, int on)
{
    ctrl_shadow = on ? (ctrl_shadow | CTRL_ON) : (ctrl_shadow & ~CTRL_ON);
    HW_REG(base, S7_CTRL) = ctrl_shadow;
}

uint32_t seg7_shown_bcd(uintptr_t base)
{
    return HW_REG(base, S7_STATUS) & 0xFFFFu;
}
