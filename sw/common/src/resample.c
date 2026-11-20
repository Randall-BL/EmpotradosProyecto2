#include "resample.h"

void resample_init(resample_t *r, uint32_t in_rate)
{
    if (in_rate == 0u || in_rate > RESAMPLE_OUT_RATE)
        in_rate = RESAMPLE_OUT_RATE;
    r->in_rate = in_rate;
    r->phase = 0;
    r->a[0] = r->a[1] = 0;
    r->has_a = 0;
}

static int16_t lerp(int16_t a, int16_t b, uint32_t w_q15)
{
    int32_t d = (int32_t)b - (int32_t)a;              /* -65535..65535 */
    return (int16_t)((int32_t)a + (int32_t)(((int64_t)d * (int64_t)w_q15) >> 15));
}

uint32_t resample_run(resample_t *r, const int16_t *in, uint32_t in_frames,
                      uint32_t channels, uint32_t *consumed,
                      uint32_t *out, uint32_t out_max)
{
    uint32_t used = 0, made = 0;
    uint32_t step = channels == 1u ? 1u : 2u;

    if (!r->has_a && in_frames > 0u) {
        r->a[0] = in[0];
        r->a[1] = in[step - 1u];
        r->has_a = 1;
        used = 1;
    }
    while (used < in_frames) {
        const int16_t *b = &in[used * step];
        int16_t bl = b[0], br = b[step - 1u];
        /* Cuadros de salida entre a (fase 0) y b (fase RESAMPLE_OUT_RATE). */
        while (r->phase < RESAMPLE_OUT_RATE) {
            if (made == out_max)
                goto done;
            uint32_t w = (r->phase << 15) / RESAMPLE_OUT_RATE;   /* < 32768 */
            out[made++] = resample_pack(lerp(r->a[0], bl, w), lerp(r->a[1], br, w));
            r->phase += r->in_rate;
        }
        r->phase -= RESAMPLE_OUT_RATE;
        r->a[0] = bl;
        r->a[1] = br;
        used++;
    }
done:
    *consumed = used;
    return made;
}

uint32_t resample_out_frames(uint32_t in_frames, uint32_t in_rate)
{
    if (in_rate == 0u)
        return 0;
    return (uint32_t)(((uint64_t)in_frames * RESAMPLE_OUT_RATE) / in_rate);
}
