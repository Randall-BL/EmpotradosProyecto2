#include "dsp.h"

void dsp_fir_init(dsp_fir_t *f, const int16_t *h, uint32_t ntaps)
{
    f->h = h;
    f->ntaps = ntaps > DSP_FIR_MAX_TAPS ? DSP_FIR_MAX_TAPS : ntaps;
    f->pos = 0;
    f->sat = 0;
    for (uint32_t i = 0; i < DSP_FIR_MAX_TAPS; ++i)
        f->dl[i] = 0;
}

int16_t dsp_fir_step(dsp_fir_t *f, int16_t x, int bypass)
{
    /* La linea de retardo se actualiza siempre, tambien en bypass (como el HW):
     * al volver al filtro no hay transitorio por historia vieja. */
    f->pos = (f->pos + DSP_FIR_MAX_TAPS - 1u) & (DSP_FIR_MAX_TAPS - 1u);
    f->dl[f->pos] = x;
    if (bypass)
        return x;

    int64_t acc = 0;
    uint32_t p = f->pos;
    for (uint32_t k = 0; k < f->ntaps; ++k) {
        acc += (int32_t)f->h[k] * (int32_t)f->dl[p];
        p = (p + 1u) & (DSP_FIR_MAX_TAPS - 1u);
    }
    int64_t r = (acc + (1 << 14)) >> 15;
    int16_t y = dsp_sat16(r);
    if (y != r)
        f->sat = 1;
    return y;
}

int16_t dsp_gain(int16_t y, uint32_t gain_q15, uint32_t *sat)
{
    int64_t z = ((int64_t)y * (int64_t)(gain_q15 & 0xFFFFu) + (1 << 14)) >> 15;
    int16_t s = dsp_sat16(z);
    if (s != z && sat)
        *sat = 1;
    return s;
}

void dsp_reverb_init(dsp_reverb_t *r, int16_t *buf, uint32_t len)
{
    r->buf = buf;
    r->len = len;
    r->pos = 0;
    for (uint32_t i = 0; i < len; ++i)
        buf[i] = 0;
}

int16_t dsp_reverb_step(dsp_reverb_t *r, int16_t x)
{
    /* |y| <= max|x|: x/2 + y/2 con |y[n-D]| acotado no puede desbordar. */
    int16_t y = (int16_t)((x >> 1) + (r->buf[r->pos] >> 1));
    r->buf[r->pos] = y;
    if (++r->pos == r->len)
        r->pos = 0;
    return y;
}

void dsp_biquad_init(dsp_biquad_t *q, int16_t b0, int16_t b1, int16_t b2, int16_t a1, int16_t a2)
{
    q->b0 = b0; q->b1 = b1; q->b2 = b2; q->a1 = a1; q->a2 = a2;
    q->x1 = q->x2 = q->y1 = q->y2 = 0;
}

int16_t dsp_biquad_step(dsp_biquad_t *q, int16_t x)
{
    /* 64 bits: cinco productos Q2.14 x Q1.15 pueden pasar de 2^31. */
    int64_t acc = (int64_t)q->b0 * x + (int64_t)q->b1 * q->x1 + (int64_t)q->b2 * q->x2
                - (int64_t)q->a1 * q->y1 - (int64_t)q->a2 * q->y2;
    int16_t y = dsp_sat16((acc + (1 << 13)) >> 14);
    q->x2 = q->x1; q->x1 = x;
    q->y2 = q->y1; q->y1 = y;
    return y;
}
