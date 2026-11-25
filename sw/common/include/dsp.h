/*
 * Filtros de audio en software, punto fijo Q1.15 (SC-10). Portable (sw/common/):
 * sin system.h ni registros, sin malloc; se prueba en el PC (tests/test_dsp.c).
 *
 *  - dsp_fir:    modelo del FIR del modulo propio, igual bit a bit al hardware
 *                (acumulador de 64 bits, y = sat16((acc + 2^14) >> 15), CD-14). Sirve de
 *                referencia para la autoprueba (NV-15) y para medir el costo en software
 *                (CD-12, RP-04).
 *  - dsp_gain:   la misma etapa de ganancia del hardware (g Q1.15 sin signo, 0..2.0).
 *  - dsp_reverb: reverberacion por un comb con realimentacion de 1/2:
 *                y[n] = x[n]/2 + y[n-D]/2. Solo sumas y desplazamientos: no necesita
 *                multiplicador (el Nios V/m es RV32I). Es el tercer filtro (CD-12).
 *  - dsp_biquad: seccion de segundo orden en forma directa I, coeficientes Q2.14;
 *                se usa para comparar el costo de un IIR en el Nios V (CD-12).
 */
#ifndef COMMON_DSP_H
#define COMMON_DSP_H

#include <stdint.h>

#define DSP_FIR_MAX_TAPS 128u              /* tamano de la linea de retardo (potencia de 2) */

/* Satura un valor de 64 bits al rango de int16_t (-32768..32767). */
static inline int16_t dsp_sat16(int64_t v)
{
    return (int16_t)(v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : v));
}

/* ---- FIR (modelo del hardware) ---- */
typedef struct {
    const int16_t *h;                      /* coeficientes Q1.15 (no se copian) */
    uint32_t ntaps;                        /* etapas usadas (<= DSP_FIR_MAX_TAPS) */
    uint32_t pos;                          /* indice de la muestra mas reciente */
    int16_t dl[DSP_FIR_MAX_TAPS];          /* linea de retardo circular */
    uint32_t sat;                          /* 1 si alguna salida satura (pegajoso) */
} dsp_fir_t;

/* Prepara el FIR con ntaps coeficientes h; la linea de retardo arranca en cero. */
void dsp_fir_init(dsp_fir_t *f, const int16_t *h, uint32_t ntaps);
/* Procesa una muestra. Con bypass devuelve x pero igual la guarda en la linea. */
int16_t dsp_fir_step(dsp_fir_t *f, int16_t x, int bypass);

/* ---- Ganancia (igual al hardware) ---- */
/* y * gain_q15 / 32768 con redondeo y saturacion; si satura y sat != NULL, *sat = 1. */
int16_t dsp_gain(int16_t y, uint32_t gain_q15, uint32_t *sat);

/* ---- Reverberacion por comb ---- */
typedef struct {
    int16_t *buf;                          /* lo provee el llamador (sin malloc) */
    uint32_t len;                          /* D en muestras */
    uint32_t pos;                          /* posicion de y[n-D] en buf */
} dsp_reverb_t;

/* Prepara la reverberacion con un buffer de len muestras (lo pone en cero). */
void dsp_reverb_init(dsp_reverb_t *r, int16_t *buf, uint32_t len);
/* y[n] = x[n]/2 + y[n-D]/2: procesa una muestra y devuelve la salida. */
int16_t dsp_reverb_step(dsp_reverb_t *r, int16_t x);

/* ---- Biquad forma directa I, coeficientes Q2.14 ---- */
typedef struct {
    int16_t b0, b1, b2, a1, a2;            /* y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2 */
    int16_t x1, x2, y1, y2;                /* historia: x[n-1], x[n-2], y[n-1], y[n-2] */
} dsp_biquad_t;

/* Fija los coeficientes Q2.14 y borra la historia. */
void dsp_biquad_init(dsp_biquad_t *q, int16_t b0, int16_t b1, int16_t b2, int16_t a1, int16_t a2);
/* Procesa una muestra con la seccion biquad. */
int16_t dsp_biquad_step(dsp_biquad_t *q, int16_t x);

#endif /* COMMON_DSP_H */
