/*
 * Pruebas del productor del HPS (HP-06, HP-07, HP-09) en el PC: recorre la carpeta de
 * WAV de prueba (validos y corruptos), publica la tabla en un shm en RAM y entrega las
 * pistas al consumidor del Nios V (shm_nios_*), intercalados como en la tarjeta.
 *
 * Uso: test_producer <carpeta con los WAV de tests/data/gen_wavs.py>
 */
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "producer.h"
#include "test.h"

static shm_t shm;
static producer_t prod;
static const char *data_dir;
static uint32_t got[48000 * 4];

static int find_track(const char *title)
{
    for (uint32_t i = 0; i < shm.ctrl.track_count; ++i)
        if (strcmp(shm.tracks[i].title, title) == 0)
            return (int)i;
    return -1;
}

/* Reproduce la pista hasta EOF leyendo de a poco. Devuelve los cuadros recibidos. */
static uint32_t play(uint32_t track, uint32_t *final_state)
{
    volatile shm_t *s = &shm;
    uint32_t seq = shm_nios_send(s, SHM_CMD_PLAY, track);
    uint32_t n = 0, iter = 0, st;
    while ((st = shm_nios_stream(s, seq)) != SHM_STREAM_EOF && st != SHM_STREAM_ERROR &&
           iter++ < 1000000u) {
        producer_poll(&prod);
        uint32_t room = (uint32_t)(sizeof got / sizeof got[0]) - n;
        n += shm_nios_read(s, seq, &got[n], room < 700u ? room : 700u);
    }
    *final_state = st;
    return n;
}

static void test_scan(void)
{
    uint32_t wavs = 0;
    DIR *d = opendir(data_dir);
    CHECK(d != NULL);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL)
        if (strstr(e->d_name, ".wav"))
            wavs++;
    if (d)
        closedir(d);

    uint32_t n = producer_scan(&prod, data_dir);
    CHECK(n >= 10u);                                   /* al menos 10 pistas validas */
    CHECK_EQ(n + prod.skipped, wavs);                  /* el resto se salta (HP-09) */
    CHECK_EQ(prod.hps_state, SHM_HPS_READY);
    for (uint32_t i = 1; i < n; ++i)                   /* orden alfabetico estable */
        CHECK(strcmp(prod.files[i - 1].path, prod.files[i].path) < 0);

    producer_publish(&prod, &shm);
    CHECK_EQ(shm_nios_check(&shm), ERR_OK);
    CHECK_EQ(shm.ctrl.hps_state, SHM_HPS_READY);
    CHECK_EQ(shm.ctrl.track_count, n);
    CHECK(find_track("Tono 44.1k") >= 0);
    CHECK(find_track("no_meta") >= 0);                 /* sin metadatos: nombre */
    int t = find_track("Tono 44.1k");
    if (t >= 0) {
        CHECK_STR(shm.tracks[t].artist, "Artista");
        CHECK_EQ(shm.tracks[t].sample_rate, 44100);
        CHECK_EQ(shm.tracks[t].frames, 48000);         /* 1 s a 48 kHz */
    }
}

/* Cada pista valida llega completa (con el retardo de 1 cuadro del remuestreo). */
static void test_play_all(void)
{
    for (uint32_t i = 0; i < shm.ctrl.track_count; ++i) {
        uint32_t st;
        uint32_t n = play(i, &st);
        CHECK_EQ(st, SHM_STREAM_EOF);
        uint32_t exp = shm.tracks[i].frames;
        uint32_t tol = 48000u / shm.tracks[i].sample_rate + 1u;
        if (n > exp || n + tol < exp)
            printf("  pista %u (%s): %u cuadros, se esperaban %u\n", i,
                   shm.tracks[i].title, n, exp);
        CHECK(n <= exp && n + tol >= exp);
    }
}

/* A 48 kHz mono el ring trae las muestras del archivo, duplicadas en L y R. */
static void test_content_48k(void)
{
    int t = -1;
    for (uint32_t i = 0; i < shm.ctrl.track_count; ++i)
        if (strstr(prod.files[i].path, "pcm16_48k_mono.wav"))
            t = (int)i;
    CHECK(t >= 0);
    if (t < 0)
        return;
    uint32_t st;
    uint32_t n = play((uint32_t)t, &st);

    FILE *f = fopen(prod.files[t].path, "rb");
    CHECK(f != NULL);
    if (!f)
        return;
    int16_t *pcm = malloc(sizeof(int16_t) * prod.files[t].frames);
    CHECK(fseek(f, (long)prod.files[t].data_offset, SEEK_SET) == 0);
    size_t rd = fread(pcm, 2, prod.files[t].frames, f);   /* el PC tambien es LE */
    fclose(f);
    CHECK_EQ(rd, prod.files[t].frames);
    uint32_t diff = 0;
    for (uint32_t i = 0; i < n; ++i)
        diff += got[i] != resample_pack(pcm[i], pcm[i]);
    CHECK_EQ(diff, 0);
    free(pcm);
}

/* STOP a media pista, PLAY de un indice invalido y cambio de pista sin terminar. */
static void test_commands(void)
{
    volatile shm_t *s = &shm;
    uint32_t seq = shm_nios_send(s, SHM_CMD_PLAY, 0);
    producer_poll(&prod);
    CHECK(shm_nios_acked(s, seq));
    CHECK(shm_nios_avail(s, seq) > 0);

    seq = shm_nios_send(s, SHM_CMD_STOP, 0);
    producer_poll(&prod);
    CHECK(shm_nios_acked(s, seq));
    CHECK_EQ(shm_nios_avail(s, seq), 0);
    CHECK_EQ(shm_nios_stream(s, seq), SHM_STREAM_IDLE);
    CHECK(!prod.active);

    seq = shm_nios_send(s, SHM_CMD_PLAY, 99);
    producer_poll(&prod);
    CHECK_EQ(shm_nios_stream(s, seq), SHM_STREAM_ERROR);

    seq = shm_nios_send(s, SHM_CMD_PLAY, 1);
    producer_poll(&prod);
    seq = shm_nios_send(s, SHM_CMD_PLAY, 2);           /* siguiente sin leer nada */
    producer_poll(&prod);
    CHECK(shm_nios_acked(s, seq));
    CHECK_EQ(shm_nios_stream(s, seq), SHM_STREAM_RUNNING);
    uint32_t st;
    uint32_t n = play(2, &st);
    CHECK_EQ(st, SHM_STREAM_EOF);
    CHECK(n > 0);
}

/* Sin SD (carpeta inexistente) y SD sin WAV. */
static void test_no_sd(void)
{
    static producer_t p2;
    static shm_t s2;
    CHECK_EQ(producer_scan(&p2, "/no/existe/esta/carpeta"), 0);
    CHECK_EQ(p2.hps_state, SHM_HPS_NO_SD);
    producer_publish(&p2, &s2);
    CHECK_EQ(shm_nios_check(&s2), ERR_OK);
    CHECK_EQ(s2.ctrl.hps_state, SHM_HPS_NO_SD);
    CHECK_EQ(s2.ctrl.track_count, 0);

    CHECK_EQ(producer_scan(&p2, "."), 0);              /* tests/: sin .wav */
    CHECK_EQ(p2.hps_state, SHM_HPS_NO_TRACKS);
}

int main(int argc, char **argv)
{
    data_dir = argc > 1 ? argv[1] : "../build/tests/data";
    RUN(test_scan);
    RUN(test_play_all);
    RUN(test_content_48k);
    RUN(test_commands);
    RUN(test_no_sd);
    producer_stop(&prod);
    return TEST_REPORT();
}
