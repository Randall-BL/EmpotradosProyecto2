/*
 * Pruebas en el PC de los filtros en software (sw/common/src/dsp.c, SC-10).
 *  - El FIR es igual bit a bit al hardware: se compara contra los vectores dorados de
 *    sim/vectors/ (los mismos del testbench del modulo propio), generados por
 *    scripts/design_fir.py.
 *  - La reverberacion: respuesta al impulso (ecos de 1/2, 1/4, ... cada D muestras) y
 *    sin desborde con entrada a fondo de escala.
 *  - El biquad: ganancia en continua de un pasa-bajos.
 * Compilar y correr:  make -C tests
 */
#include <stdio.h>
#include <stdlib.h>
#include "dsp.h"
#include "fir_coefs.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static const char *const CASES[] = {
    "impulso", "escalon", "senos", "saturacion", "ruido_banco1", "bypass_gain", "taps16",
    "ganancia_alta",
};

static void test_fir_vectors(void)
{
    for (unsigned c = 0; c < sizeof CASES / sizeof CASES[0]; ++c) {
        char path[128];
        snprintf(path, sizeof path, "../sim/vectors/fir_%s.txt", CASES[c]);
        FILE *fp = fopen(path, "r");
        if (!fp) { printf("FALLO: no se pudo abrir %s\n", path); failures++; continue; }
        unsigned bank, ntaps, gain, bypass, n;
        if (fscanf(fp, "%u %u %u %u %u", &bank, &ntaps, &gain, &bypass, &n) != 5) {
            printf("FALLO: cabecera de %s\n", path); failures++; fclose(fp); continue;
        }
        dsp_fir_t l, r;
        dsp_fir_init(&l, fir_coefs[bank], ntaps);
        dsp_fir_init(&r, fir_coefs[bank], ntaps);
        unsigned bad = 0;
        for (unsigned i = 0; i < n; ++i) {
            unsigned in, out;
            if (fscanf(fp, "%x %x", &in, &out) != 2) { bad++; break; }
            uint32_t sat = 0;
            int16_t yl = dsp_gain(dsp_fir_step(&l, (int16_t)(in & 0xFFFF), (int)bypass), gain, &sat);
            int16_t yr = dsp_gain(dsp_fir_step(&r, (int16_t)(in >> 16), (int)bypass), gain, &sat);
            if ((uint16_t)yl != (out & 0xFFFF) || (uint16_t)yr != (out >> 16))
                bad++;
        }
        fclose(fp);
        if (bad) printf("FALLO: fir_%s: %u muestras distintas del modelo\n", CASES[c], bad);
        failures += bad != 0;
    }
}

static void test_reverb(void)
{
    static int16_t buf[100];
    dsp_reverb_t r;
    dsp_reverb_init(&r, buf, 100);
    CHECK(dsp_reverb_step(&r, 16000) == 8000);              /* x/2 */
    for (int i = 1; i < 100; ++i) CHECK(dsp_reverb_step(&r, 0) == 0);
    CHECK(dsp_reverb_step(&r, 0) == 4000);                  /* eco a D: 1/4 */
    for (int i = 1; i < 100; ++i) (void)dsp_reverb_step(&r, 0);
    CHECK(dsp_reverb_step(&r, 0) == 2000);                  /* eco a 2D: 1/8 */

    dsp_reverb_init(&r, buf, 100);
    int16_t mx = 0, mn = 0;
    for (int i = 0; i < 5000; ++i) {                        /* fondo de escala sostenido */
        int16_t y = dsp_reverb_step(&r, (i & 1) ? INT16_MIN : INT16_MAX);
        if (y > mx) mx = y;
        if (y < mn) mn = y;
    }
    CHECK(mx > 16000 && mn < -16000);                       /* sin desborde: sigue la entrada */
}

static void test_biquad(void)
{
    /* Pasa-bajos de 1er orden escrito como biquad: y = 0.25x + 0.25x1 + 0.5y1 (Q2.14). */
    dsp_biquad_t q;
    dsp_biquad_init(&q, 4096, 4096, 0, -8192, 0);
    int16_t y = 0;
    for (int i = 0; i < 200; ++i) y = dsp_biquad_step(&q, 10000);
    CHECK(y >= 9998 && y <= 10002);                         /* ganancia DC = 1 */
    dsp_biquad_init(&q, 16384, 16384, 16384, 0, 0);         /* 3 x fondo de escala: satura */
    for (int i = 0; i < 4; ++i) y = dsp_biquad_step(&q, 30000);
    CHECK(y == INT16_MAX);
}

int main(void)
{
    test_fir_vectors();
    test_reverb();
    test_biquad();
    if (failures) {
        printf("test_dsp: FAIL (%d)\n", failures);
        return EXIT_FAILURE;
    }
    printf("test_dsp: PASS (FIR igual bit a bit a los 8 casos de sim/vectors)\n");
    return EXIT_SUCCESS;
}
