#include "audio.h"
#include "hw.h"

#define AUDIO_REG_CONTROL   0x0
#define AUDIO_REG_FIFOSPACE 0x4
#define AUDIO_REG_LEFT      0x8
#define AUDIO_REG_RIGHT     0xC

#define AUDIO_CTRL_WE (1u << 1)
#define AUDIO_CTRL_CW (1u << 3)

void audio_init(uintptr_t base)
{
    /* CW limpia las FIFO de salida mientras vale 1: se sube y se baja. */
    HW_REG(base, AUDIO_REG_CONTROL) = AUDIO_CTRL_CW;
    HW_REG(base, AUDIO_REG_CONTROL) = 0;
}

uint32_t audio_space(uintptr_t base)
{
    uint32_t fs = HW_REG(base, AUDIO_REG_FIFOSPACE);
    uint32_t wslc = (fs >> 24) & 0xFFu;
    uint32_t wsrc = (fs >> 16) & 0xFFu;
    /* Las dos FIFO avanzan juntas, pero se toma la menor por seguridad. */
    return wslc < wsrc ? wslc : wsrc;
}

void audio_write_frame(uintptr_t base, int16_t left, int16_t right)
{
    /* El IP toma los 16 bits bajos de cada registro (dato justificado a la derecha). */
    HW_REG(base, AUDIO_REG_LEFT)  = (uint32_t)(uint16_t)left;
    HW_REG(base, AUDIO_REG_RIGHT) = (uint32_t)(uint16_t)right;
}

uint32_t audio_write(uintptr_t base, const int16_t *lr, uint32_t frames)
{
    uint32_t n = audio_space(base);
    if (n > frames)
        n = frames;
    for (uint32_t i = 0; i < n; ++i) {
        audio_write_frame(base, lr[0], lr[1]);
        lr += 2;
    }
    return n;
}

void audio_irq_enable(uintptr_t base, int enable)
{
    HW_REG(base, AUDIO_REG_CONTROL) = enable ? AUDIO_CTRL_WE : 0;
}
