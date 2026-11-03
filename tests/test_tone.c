/*
 * Pruebas en el PC del generador de tono (sw/common/src/tone.c).
 * Compilar y correr:  make -C tests
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "tone.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

int main(void)
{
    /* 440 Hz a 48 kHz durante 1 s: 440 cruces por cero de subida (+-1). */
    tone_t t;
    tone_init(&t, 440, 48000);
    int crossings = 0, maxv = 0, minv = 0;
    int16_t prev = tone_next(&t);
    double err2 = 0;
    for (int n = 1; n < 48000; ++n) {
        int16_t s = tone_next(&t);
        if (prev < 0 && s >= 0)
            ++crossings;
        if (s > maxv) maxv = s;
        if (s < minv) minv = s;
        double ideal = 16384.0 * sin(2 * M_PI * 440.0 * n / 48000.0);
        err2 += (s - ideal) * (s - ideal);
        prev = s;
    }
    CHECK(crossings >= 439 && crossings <= 441);
    CHECK(maxv == 16384 && minv == -16384);
    /* Error de la tabla sin interpolacion: SNR > 40 dB frente al seno ideal. */
    double snr = 10 * log10((16384.0 * 16384.0 / 2) / (err2 / 47999));
    CHECK(snr > 40.0);
    printf("tono 440 Hz: %d cruces/s, SNR de la tabla %.1f dB\n", crossings, snr);

    /* Incremento de fase exacto: f * 2^32 / fs. */
    tone_init(&t, 1000, 48000);
    CHECK(t.step == (uint32_t)((1000ull << 32) / 48000));

    if (failures) {
        printf("test_tone: FAIL (%d)\n", failures);
        return EXIT_FAILURE;
    }
    printf("test_tone: PASS\n");
    return EXIT_SUCCESS;
}
