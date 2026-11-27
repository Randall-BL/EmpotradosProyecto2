/*
 * Conversion de frecuencia de muestreo a 48 kHz por interpolacion lineal (HP-07).
 * Ver resample.h.
 *
 * Modelo: entre dos muestras de entrada consecutivas a (fase 0) y b (fase 48000) se
 * generan las muestras de salida que caen en ese intervalo. Cada muestra de salida
 * avanza la fase en in_rate; cuando la fase llega a 48000 se pasa al siguiente par
 * (a <- b). Asi, por cada muestra de entrada salen 48000 / in_rate muestras de salida
 * en promedio (6 a 8 kHz, 3 a 16 kHz, ~1.088 a 44.1 kHz, 1 a 48 kHz).
 */
#include "resample.h"

/*
 * Prepara la conversion. Una frecuencia invalida (0 o mayor a 48 kHz) se trata como
 * 48 kHz (copia directa). La primera muestra que llegue sera el punto a inicial.
 */
void resample_init(resample_t *r, uint32_t in_rate)
{
    if (in_rate == 0u || in_rate > RESAMPLE_OUT_RATE)
        in_rate = RESAMPLE_OUT_RATE;
    r->in_rate = in_rate;
    r->phase = 0;
    r->a[0] = r->a[1] = 0;
    r->has_a = 0;
}

/*
 * Interpolacion lineal entre a y b: a + (b - a) * w, con el peso w en Q15
 * (0 = a, 32768 = b). La diferencia se calcula en 32 bits porque puede llegar a
 * +-65535, y el producto en 64 bits.
 */
static int16_t lerp(int16_t a, int16_t b, uint32_t w_q15)
{
    int32_t d = (int32_t)b - (int32_t)a;              /* -65535..65535 */
    return (int16_t)((int32_t)a + (int32_t)(((int64_t)d * (int64_t)w_q15) >> 15));
}

/*
 * Convierte un bloque. Ver resample.h para los parametros. El estado (fase y ultima
 * muestra) queda guardado en r, asi la siguiente llamada continua exactamente donde
 * quedo esta, aunque el bloque se haya cortado por falta de espacio en la salida.
 */
uint32_t resample_run(resample_t *r, const int16_t *in, uint32_t in_frames,
                      uint32_t channels, uint32_t *consumed,
                      uint32_t *out, uint32_t out_max)
{
    uint32_t used = 0, made = 0;           /* cuadros de entrada usados / de salida hechos */
    /* Distancia entre cuadros en el arreglo intercalado. Con mono, step = 1 y
     * b[step - 1] = b[0]: el mismo valor va a los dos canales. */
    uint32_t step = channels == 1u ? 1u : 2u;

    /* La primera muestra de todo el archivo solo sirve de punto de partida a. */
    if (!r->has_a && in_frames > 0u) {
        r->a[0] = in[0];
        r->a[1] = in[step - 1u];
        r->has_a = 1;
        used = 1;
    }
    while (used < in_frames) {
        /* b = siguiente muestra de entrada (extremo derecho del intervalo). */
        const int16_t *b = &in[used * step];
        int16_t bl = b[0], br = b[step - 1u];
        /* Cuadros de salida entre a (fase 0) y b (fase RESAMPLE_OUT_RATE). */
        while (r->phase < RESAMPLE_OUT_RATE) {
            if (made == out_max)
                goto done;                 /* salida llena: b no se consume todavia */
            uint32_t w = (r->phase << 15) / RESAMPLE_OUT_RATE;   /* < 32768 */
            out[made++] = resample_pack(lerp(r->a[0], bl, w), lerp(r->a[1], br, w));
            r->phase += r->in_rate;
        }
        /* Se paso de b: b se vuelve el nuevo a y se consume. */
        r->phase -= RESAMPLE_OUT_RATE;
        r->a[0] = bl;
        r->a[1] = br;
        used++;
    }
done:
    *consumed = used;
    return made;
}

/* Cuadros de salida (48 kHz) que se obtienen de in_frames cuadros a in_rate Hz:
 * in_frames * 48000 / in_rate, en 64 bits para no desbordar. 0 si in_rate = 0. */
uint32_t resample_out_frames(uint32_t in_frames, uint32_t in_rate)
{
    if (in_rate == 0u)
        return 0;
    return (uint32_t)(((uint64_t)in_frames * RESAMPLE_OUT_RATE) / in_rate);
}
