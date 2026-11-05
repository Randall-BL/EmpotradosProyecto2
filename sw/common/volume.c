#include "volume.h"

/* Generada por scripts/gen_volume_table.py: 32 escalones de 2 dB,
 * de -60 dB (escalon 1) a 0 dB; el escalon 0 es silencio. */
static const uint16_t volume_gain_q15[VOLUME_STEPS] = {
        0,    33,    41,    52,    65,    82,   104,   130,
      164,   207,   260,   328,   413,   519,   654,   823,
     1036,  1305,  1642,  2068,  2603,  3277,  4125,  5193,
     6538,  8231, 10362, 13045, 16423, 20675, 26029, 32767,
};

/* Cada escalon cubre 4096 / 32 = 128 cuentas del ADC. */
#define COUNTS_PER_STEP ((VOLUME_ADC_MAX + 1u) / VOLUME_STEPS)

static uint32_t step_of(uint32_t adc)
{
    if (adc > VOLUME_ADC_MAX)
        adc = VOLUME_ADC_MAX;
    return adc / COUNTS_PER_STEP;     /* potencia de 2: el compilador usa un shift */
}

uint32_t volume_gain_of_step(uint32_t step)
{
    if (step >= VOLUME_STEPS)
        step = VOLUME_STEPS - 1u;
    return volume_gain_q15[step];
}

void volume_init(volume_t *v, uint32_t adc)
{
    v->step = step_of(adc);
    v->gain_q15 = volume_gain_of_step(v->step);
}

int volume_update(volume_t *v, uint32_t adc)
{
    uint32_t s = step_of(adc);
    if (s == v->step)
        return 0;

    /* Histeresis: solo se cambia si la lectura entra HYSTERESIS cuentas en el
     * escalon nuevo; el ruido del ADC en un borde no hace saltar el volumen. */
    uint32_t lo = v->step * COUNTS_PER_STEP;
    uint32_t hi = lo + COUNTS_PER_STEP;
    if (adc >= hi && adc < hi + VOLUME_HYSTERESIS)
        return 0;
    if (adc < lo && adc + VOLUME_HYSTERESIS >= lo)
        return 0;

    v->step = s;
    v->gain_q15 = volume_gain_of_step(s);
    return 1;
}

uint32_t volume_percent(const volume_t *v)
{
    /* Sin division ni multiplicacion por hardware en el Nios V/m: 100/31 ~= 3.2,
     * se usa (step * 13 + 2) >> 2 = step * 3.25, que da 100 % en el escalon 31. */
    uint32_t p = (v->step * 13u + 2u) >> 2;
    return p > 100u ? 100u : p;
}

int16_t volume_scale(int16_t s, uint32_t gain_q15)
{
    int32_t y = ((int32_t)s * (int32_t)gain_q15 + (1 << 14)) >> 15;
    if (y > INT16_MAX)
        y = INT16_MAX;
    else if (y < INT16_MIN)
        y = INT16_MIN;
    return (int16_t)y;
}

void volume_apply(int16_t *samples, uint32_t n, uint32_t gain_q15)
{
    for (uint32_t i = 0; i < n; ++i)
        samples[i] = volume_scale(samples[i], gain_q15);
}
