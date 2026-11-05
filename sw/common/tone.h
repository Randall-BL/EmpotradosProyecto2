/*
 * Generador de tono senoidal de prueba (IN-02, IN-03): sintesis por tabla con un
 * acumulador de fase de 32 bits, sin multiplicaciones ni divisiones en tiempo de
 * ejecucion (el Nios V/m es RV32I). Portable: se prueba en el PC (tests/test_tone.c).
 */
#ifndef COMMON_TONE_H
#define COMMON_TONE_H

#include <stdint.h>

#define TONE_TABLE_SIZE 1024u   /* 2 KB; sin interpolacion la SNR es ~49 dB */

typedef struct {
    uint32_t phase;
    uint32_t step;              /* f * 2^32 / fs */
} tone_t;

/* Prepara un tono de freq_hz a fs_hz. Hace una division, solo al configurar. */
void tone_init(tone_t *t, uint32_t freq_hz, uint32_t fs_hz);

/* Siguiente muestra (amplitud -6 dBFS). */
int16_t tone_next(tone_t *t);

#endif /* COMMON_TONE_H */
