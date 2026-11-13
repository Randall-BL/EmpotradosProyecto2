/*
 * Pruebas del remuestreador a 48 kHz del productor del HPS (HP-07, CD-07).
 */
#include <stdint.h>
#include "resample.h"
#include "test.h"

#define N_IN 4410u

static int16_t in_st[N_IN * 2];
static int16_t in_mono[N_IN];
static uint32_t out_a[N_IN * 7];
static uint32_t out_b[N_IN * 7];

static int16_t left(uint32_t f)  { return (int16_t)f; }
static int16_t right(uint32_t f) { return (int16_t)(f >> 16); }

static void fill(void)
{
    for (uint32_t i = 0; i < N_IN; ++i) {
        in_st[2 * i] = (int16_t)(i * 7u);
        in_st[2 * i + 1] = (int16_t)(-(int32_t)(i * 5u));
        in_mono[i] = (int16_t)(i * 3u);
    }
}

/* A 48 kHz es una copia (con un cuadro de retardo). */
static void test_copy_48k(void)
{
    resample_t r;
    uint32_t used;
    resample_init(&r, 48000);
    uint32_t n = resample_run(&r, in_st, N_IN, 2, &used, out_a, N_IN * 7);
    CHECK_EQ(used, N_IN);
    CHECK_EQ(n, N_IN - 1u);
    for (uint32_t i = 0; i < n; ++i) {
        CHECK_EQ(left(out_a[i]), in_st[2 * i]);
        CHECK_EQ(right(out_a[i]), in_st[2 * i + 1]);
    }
}

/* 16 kHz: tres cuadros por muestra, interpolados en tercios. */
static void test_interp_16k(void)
{
    resample_t r;
    uint32_t used;
    const int16_t in[] = {0, 0, 3000, -3000, 6000, -6000};
    resample_init(&r, 16000);
    uint32_t n = resample_run(&r, in, 3, 2, &used, out_a, 16);
    CHECK_EQ(used, 3);
    CHECK_EQ(n, 6);
    const int16_t exp_l[] = {0, 1000, 2000, 3000, 4000, 5000};
    for (uint32_t i = 0; i < 6; ++i) {
        CHECK(left(out_a[i]) >= exp_l[i] - 1 && left(out_a[i]) <= exp_l[i]);
        CHECK(right(out_a[i]) <= -exp_l[i] + 1 && right(out_a[i]) >= -exp_l[i]);
    }
}

/* Cantidad de cuadros: 8 kHz x6, 44.1 kHz x160/147. */
static void test_counts(void)
{
    resample_t r;
    uint32_t used;
    resample_init(&r, 8000);
    uint32_t n = resample_run(&r, in_st, 1000, 2, &used, out_a, N_IN * 7);
    CHECK_EQ(used, 1000);
    CHECK_EQ(n, 999u * 6u);

    resample_init(&r, 44100);
    n = resample_run(&r, in_st, N_IN, 2, &used, out_a, N_IN * 7);
    CHECK_EQ(used, N_IN);
    uint32_t exp = resample_out_frames(N_IN, 44100);       /* 4800 */
    CHECK_EQ(exp, 4800);
    CHECK(n + 2u >= exp && n <= exp);
}

/* Mono: el mismo valor en los dos canales. */
static void test_mono(void)
{
    resample_t r;
    uint32_t used;
    resample_init(&r, 48000);
    uint32_t n = resample_run(&r, in_mono, 100, 1, &used, out_a, 200);
    CHECK_EQ(n, 99);
    for (uint32_t i = 0; i < n; ++i) {
        CHECK_EQ(left(out_a[i]), in_mono[i]);
        CHECK_EQ(right(out_a[i]), in_mono[i]);
    }
}

/* Por bloques chicos y con la salida limitada da lo mismo que de una sola vez. */
static void test_chunked(void)
{
    static const uint32_t rates[] = {8000, 16000, 44100, 48000};
    for (uint32_t k = 0; k < 4; ++k) {
        resample_t r;
        uint32_t used;
        resample_init(&r, rates[k]);
        uint32_t total = resample_run(&r, in_st, N_IN, 2, &used, out_a, N_IN * 7);

        resample_init(&r, rates[k]);
        uint32_t pos_in = 0, pos_out = 0, iter = 0;
        while (pos_in < N_IN && iter++ < 100000u) {
            uint32_t chunk = 1u + (iter * 37u) % 50u;
            if (chunk > N_IN - pos_in)
                chunk = N_IN - pos_in;
            uint32_t room = 1u + (iter * 13u) % 29u;
            pos_out += resample_run(&r, &in_st[2 * pos_in], chunk, 2, &used,
                                    &out_b[pos_out], room);
            CHECK(used <= chunk);
            pos_in += used;
        }
        CHECK_EQ(pos_out, total);
        uint32_t diff = 0;
        for (uint32_t i = 0; i < total; ++i)
            diff += out_a[i] != out_b[i];
        CHECK_EQ(diff, 0);
    }
}

/* Sin espacio de salida no consume nada. */
static void test_no_room(void)
{
    resample_t r;
    uint32_t used;
    resample_init(&r, 8000);
    (void)resample_run(&r, in_st, 1, 2, &used, out_a, 0);    /* primera muestra */
    CHECK_EQ(used, 1);
    uint32_t n = resample_run(&r, &in_st[2], 10, 2, &used, out_a, 0);
    CHECK_EQ(n, 0);
    CHECK_EQ(used, 0);
}

int main(void)
{
    fill();
    RUN(test_copy_48k);
    RUN(test_interp_16k);
    RUN(test_counts);
    RUN(test_mono);
    RUN(test_chunked);
    RUN(test_no_room);
    return TEST_REPORT();
}
