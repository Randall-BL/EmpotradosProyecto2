/*
 * Filtros de audio en software, punto fijo Q1.15 (SC-10). Ver dsp.h.
 *
 * Convenciones de punto fijo:
 *   - Q1.15: entero de 16 bits con signo que representa [-1, 1); 32768 = 1.0.
 *   - Q2.14: entero de 16 bits que representa [-2, 2); 16384 = 1.0 (biquad).
 *   - Un producto Qa x Qb queda en Q(a+b); para volver a Q1.15 se suma la mitad del
 *     ultimo bit descartado (redondeo) y se desplaza a la derecha.
 */
#include "dsp.h"

/*
 * Prepara el FIR con los coeficientes h (Q1.15) y ntaps etapas (como mucho
 * DSP_FIR_MAX_TAPS). La linea de retardo arranca en ceros y se borra la marca de
 * saturacion.
 */
void dsp_fir_init(dsp_fir_t *f, const int16_t *h, uint32_t ntaps)
{
    f->h = h;
    f->ntaps = ntaps > DSP_FIR_MAX_TAPS ? DSP_FIR_MAX_TAPS : ntaps;
    f->pos = 0;
    f->sat = 0;
    for (uint32_t i = 0; i < DSP_FIR_MAX_TAPS; ++i)
        f->dl[i] = 0;
}

/*
 * Procesa una muestra x y devuelve la salida del FIR:
 *     y[n] = sat16( (sum_k h[k] * x[n-k] + 2^14) >> 15 )
 * Es el mismo calculo bit a bit que hace hw/rtl/fir_core.v. Con bypass != 0 devuelve
 * x sin filtrar, pero igual guarda la muestra en la linea de retardo.
 */
int16_t dsp_fir_step(dsp_fir_t *f, int16_t x, int bypass)
{
    /* La linea de retardo se actualiza siempre, tambien en bypass (como el HW):
     * al volver al filtro no hay transitorio por historia vieja. */
    /* El buffer es circular de tamano potencia de 2: se retrocede pos una posicion
     * (con vuelta) y alli se guarda la muestra mas nueva. */
    f->pos = (f->pos + DSP_FIR_MAX_TAPS - 1u) & (DSP_FIR_MAX_TAPS - 1u);
    f->dl[f->pos] = x;
    if (bypass)
        return x;

    /* Convolucion: h[0] con la muestra mas nueva, h[1] con la anterior, etc. Avanzar
     * p hacia adelante en el buffer es ir hacia atras en el tiempo. */
    int64_t acc = 0;
    uint32_t p = f->pos;
    for (uint32_t k = 0; k < f->ntaps; ++k) {
        acc += (int32_t)f->h[k] * (int32_t)f->dl[p];
        p = (p + 1u) & (DSP_FIR_MAX_TAPS - 1u);
    }
    /* Q2.30 -> Q1.15 con redondeo al mas cercano y saturacion a 16 bits. */
    int64_t r = (acc + (1 << 14)) >> 15;
    int16_t y = dsp_sat16(r);
    if (y != r)
        f->sat = 1;                        /* marca pegajosa: hubo al menos una saturacion */
    return y;
}

/*
 * Etapa de ganancia, igual al hardware: y * g / 32768 con redondeo y saturacion.
 * gain_q15 es sin signo (solo se usan sus 16 bits bajos: 0..65535 = 0..~2.0). Si
 * sat no es NULL y la salida satura, se pone *sat = 1.
 */
int16_t dsp_gain(int16_t y, uint32_t gain_q15, uint32_t *sat)
{
    int64_t z = ((int64_t)y * (int64_t)(gain_q15 & 0xFFFFu) + (1 << 14)) >> 15;
    int16_t s = dsp_sat16(z);
    if (s != z && sat)
        *sat = 1;
    return s;
}

/*
 * Prepara la reverberacion con un buffer de len muestras que da el llamador. len es
 * el retardo D del eco (2400 muestras = 50 ms a 48 kHz). El buffer se pone en cero.
 */
void dsp_reverb_init(dsp_reverb_t *r, int16_t *buf, uint32_t len)
{
    r->buf = buf;
    r->len = len;
    r->pos = 0;
    for (uint32_t i = 0; i < len; ++i)
        buf[i] = 0;
}

/*
 * Un paso del filtro comb realimentado:  y[n] = x[n]/2 + y[n-D]/2.
 * buf[pos] guarda la salida de hace D muestras; se lee, se reemplaza por la salida
 * nueva y pos avanza con vuelta. Cada eco sale a la mitad de amplitud que el anterior
 * (-6 dB cada 50 ms). Solo usa desplazamientos y sumas.
 */
int16_t dsp_reverb_step(dsp_reverb_t *r, int16_t x)
{
    /* |y| <= max|x|: x/2 + y/2 con |y[n-D]| acotado no puede desbordar. */
    int16_t y = (int16_t)((x >> 1) + (r->buf[r->pos] >> 1));
    r->buf[r->pos] = y;
    if (++r->pos == r->len)
        r->pos = 0;
    return y;
}

/*
 * Prepara una seccion biquad con coeficientes Q2.14:
 *     y = b0 x + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]
 * (a0 se asume 1). La historia de entradas y salidas arranca en cero.
 */
void dsp_biquad_init(dsp_biquad_t *q, int16_t b0, int16_t b1, int16_t b2, int16_t a1, int16_t a2)
{
    q->b0 = b0; q->b1 = b1; q->b2 = b2; q->a1 = a1; q->a2 = a2;
    q->x1 = q->x2 = q->y1 = q->y2 = 0;
}

/* Procesa una muestra con la seccion biquad (forma directa I) y devuelve la salida. */
int16_t dsp_biquad_step(dsp_biquad_t *q, int16_t x)
{
    /* 64 bits: cinco productos Q2.14 x Q1.15 pueden pasar de 2^31. */
    int64_t acc = (int64_t)q->b0 * x + (int64_t)q->b1 * q->x1 + (int64_t)q->b2 * q->x2
                - (int64_t)q->a1 * q->y1 - (int64_t)q->a2 * q->y2;
    /* Q3.29 -> Q1.15: redondeo (+2^13) y desplazamiento de 14 bits. */
    int16_t y = dsp_sat16((acc + (1 << 13)) >> 14);
    /* Se corre la historia: la muestra actual pasa a ser la "anterior". */
    q->x2 = q->x1; q->x1 = x;
    q->y2 = q->y1; q->y1 = y;
    return y;
}
