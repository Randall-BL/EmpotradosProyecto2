/*
 * Pruebas en el PC del control de volumen (sw/common/src/volume.c, SC-11).
 * Compilar y correr:  make -C tests
 */
#include <stdio.h>
#include <stdlib.h>
#include "volume.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_extremos(void)
{
    volume_t v;
    volume_init(&v, 0);
    CHECK(v.step == 0 && v.gain_q15 == 0);                 /* potenciometro abajo: silencio */
    volume_init(&v, VOLUME_ADC_MAX);
    CHECK(v.step == VOLUME_STEPS - 1 && v.gain_q15 == 32767);  /* arriba: 0 dB */
    CHECK(volume_percent(&v) == 100);
    volume_init(&v, 99999);                                 /* fuera de rango: satura */
    CHECK(v.step == VOLUME_STEPS - 1);
}

static void test_monotona(void)
{
    for (uint32_t s = 1; s < VOLUME_STEPS; ++s)
        CHECK(volume_gain_of_step(s) > volume_gain_of_step(s - 1));
    /* 2 dB por escalon: razon entre escalones ~1.259 (+-2 %) desde el 10. */
    for (uint32_t s = 11; s < VOLUME_STEPS; ++s) {
        double r = (double)volume_gain_of_step(s) / volume_gain_of_step(s - 1);
        CHECK(r > 1.234 && r < 1.284);
    }
}

static void test_histeresis(void)
{
    volume_t v;
    volume_init(&v, 10 * 128 + 64);                         /* centro del escalon 10 */
    CHECK(v.step == 10);
    CHECK(volume_update(&v, 11 * 128 + 5) == 0);            /* apenas pasa el borde */
    CHECK(v.step == 10);
    CHECK(volume_update(&v, 11 * 128 + VOLUME_HYSTERESIS) == 1);
    CHECK(v.step == 11);
    CHECK(volume_update(&v, 11 * 128 - 5) == 0);            /* ruido de vuelta: no baja */
    CHECK(v.step == 11);
    CHECK(volume_update(&v, 9 * 128) == 1);                 /* movimiento real: baja */
    CHECK(v.step == 9);
    CHECK(volume_update(&v, VOLUME_ADC_MAX) == 1);          /* salto grande */
    CHECK(v.step == VOLUME_STEPS - 1);
}

static void test_escalado(void)
{
    CHECK(volume_scale(1000, 32767) == 1000);               /* 0 dB: igual (redondeo) */
    CHECK(volume_scale(-1000, 32767) == -1000);
    CHECK(volume_scale(32767, 32767) == 32766 || volume_scale(32767, 32767) == 32767);
    CHECK(volume_scale(-32768, 32767) == -32767);
    CHECK(volume_scale(12345, 0) == 0);                     /* silencio */
    CHECK(volume_scale(20000, 16384) == 10000);             /* -6 dB */
    CHECK(volume_scale(-20000, 16384) == -10000);
    CHECK(volume_scale(30000, 65535) == 32767);             /* ganancia > 1: satura arriba */
    CHECK(volume_scale(-30000, 65535) == -32768);           /* y abajo */

    int16_t buf[4] = {100, -100, 32767, -32768};
    volume_apply(buf, 4, 16384);
    CHECK(buf[0] == 50 && buf[1] == -50 && buf[2] == 16384 && buf[3] == -16384);
}

int main(void)
{
    test_extremos();
    test_monotona();
    test_histeresis();
    test_escalado();
    if (failures) {
        printf("test_volume: FAIL (%d)\n", failures);
        return EXIT_FAILURE;
    }
    printf("test_volume: PASS\n");
    return EXIT_SUCCESS;
}
