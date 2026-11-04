/*
 * Pruebas del parser WAV (SC-02..SC-06) con los archivos de tests/data/gen_wavs.py.
 * Uso: test_wav <directorio de datos>   (make -C tests los genera en build/tests/data)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "test.h"
#include "wav.h"

static const char *data_dir = "../build/tests/data";
static uint8_t file_buf[512 * 1024];

/* Lector sobre FILE*: el mismo patron que usara el HPS con FatFs (lseek + read). */
static uint32_t file_read(void *ctx, uint32_t off, uint8_t *buf, uint32_t n)
{
    FILE *f = (FILE *)ctx;
    if (fseek(f, (long)off, SEEK_SET) != 0)
        return 0;
    return (uint32_t)fread(buf, 1, n, f);
}

static err_t parse_file(const char *name, wav_info_t *info)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", data_dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("  no se pudo abrir %s\n", path);
        return ERR_IO;
    }
    fseek(f, 0, SEEK_END);
    wav_reader_t r = { f, (uint32_t)ftell(f), file_read };
    err_t e = wav_parse(&r, info);

    /* El lector en memoria debe dar exactamente lo mismo. */
    rewind(f);
    size_t n = fread(file_buf, 1, sizeof file_buf, f);
    fclose(f);
    wav_mem_t m;
    wav_reader_t rm;
    wav_info_t im;
    wav_mem_reader(&rm, &m, file_buf, (uint32_t)n);
    CHECK_EQ(wav_parse(&rm, &im), e);
    CHECK(memcmp(&im, info, sizeof im) == 0);
    return e;
}

static void test_le(void)
{
    const uint8_t b[] = { 0x34, 0x12, 0x78, 0x56, 0xFF };
    uint16_t v16 = 0;
    uint32_t v32 = 0;
    CHECK_EQ(le16(b), 0x1234);
    CHECK_EQ(le32(b), 0x56781234);
    CHECK_EQ(le_read16(b, sizeof b, 3, &v16), ERR_OK);
    CHECK_EQ(v16, 0xFF56);
    CHECK_EQ(le_read16(b, sizeof b, 4, &v16), ERR_BOUNDS);
    CHECK_EQ(le_read32(b, sizeof b, 1, &v32), ERR_OK);
    CHECK_EQ(v32, 0xFF567812u);
    CHECK_EQ(le_read32(b, sizeof b, 2, &v32), ERR_BOUNDS);
    CHECK_EQ(le_read32(b, 3, 0, &v32), ERR_BOUNDS);
    CHECK_EQ(le_read32(b, sizeof b, (size_t)-1, &v32), ERR_BOUNDS);   /* sin desborde */
    CHECK_EQ(le_read32(NULL, sizeof b, 0, &v32), ERR_ARG);
    CHECK(le_fourcc_eq((const uint8_t *)"RIFF", "RIFF"));
    CHECK(!le_fourcc_eq((const uint8_t *)"RIFX", "RIFF"));
}

static void test_valid_formats(void)
{
    wav_info_t w;
    CHECK_EQ(parse_file("pcm16_8k_mono.wav", &w), ERR_OK);
    CHECK_EQ(w.sample_rate, 8000);
    CHECK_EQ(w.channels, 1);
    CHECK_EQ(w.bits, 16);
    CHECK_EQ(w.block_align, 2);
    CHECK_EQ(w.byte_rate, 16000);
    CHECK_EQ(w.data_offset, 12 + 24 + 8 + 4 + 16 + 16 + 16 + 8);
    CHECK_EQ(w.frames, 20000);
    CHECK_EQ(w.duration_s, 2);                 /* 2.5 s -> 2 (hacia abajo) */
    CHECK_STR(w.title, "Tono 8k");
    CHECK_STR(w.artist, "Prueba");
    CHECK_STR(w.album, "Parser");
    CHECK(w.flags & WAV_FLAG_META_INFO);

    CHECK_EQ(parse_file("pcm16_16k_stereo.wav", &w), ERR_OK);
    CHECK_EQ(w.sample_rate, 16000);
    CHECK_EQ(w.channels, 2);
    CHECK_EQ(w.frames, 48000);
    CHECK_EQ(w.duration_s, 3);
    CHECK_STR(w.title, "Tono 16k");            /* LIST despues de data */
    CHECK_STR(w.album, "");

    CHECK_EQ(parse_file("pcm16_44k_stereo.wav", &w), ERR_OK);
    CHECK_EQ(w.sample_rate, 44100);
    CHECK_EQ(w.byte_rate, 176400);
    CHECK_EQ(w.duration_s, 1);
    CHECK_STR(w.title, "Tono 44.1k");

    CHECK_EQ(parse_file("pcm16_48k_mono.wav", &w), ERR_OK);
    CHECK_EQ(w.sample_rate, 48000);
    CHECK_EQ(w.frames, 24000);
    CHECK_EQ(w.duration_s, 0);

    CHECK_EQ(parse_file("extensible.wav", &w), ERR_OK);
    CHECK(w.flags & WAV_FLAG_EXTENSIBLE);
    CHECK_EQ(w.channels, 2);
    CHECK_EQ(w.frames, 9600);
}

static void test_chunks(void)
{
    wav_info_t w;
    CHECK_EQ(parse_file("extra_chunks.wav", &w), ERR_OK);
    /* JUNK(7+1) fmt(24) fact(12) bext(13+1) antes de data */
    CHECK_EQ(w.data_offset, 12 + 16 + 24 + 12 + 22 + 8);
    CHECK_EQ(w.frames, 8000);
    CHECK_STR(w.title, "Extra");
    CHECK_STR(w.album, "Impares");
    CHECK_EQ(w.flags & WAV_FLAG_TRUNCATED, 0);

    CHECK_EQ(parse_file("odd_data.wav", &w), ERR_OK);
    CHECK_EQ(w.frames, 2000);                  /* el byte suelto no forma un cuadro */
    CHECK_EQ(w.data_size, 4000);
    CHECK(w.flags & WAV_FLAG_TRUNCATED);
    CHECK_STR(w.title, "Impar");               /* el relleno se respeta */
}

static void test_metadata(void)
{
    wav_info_t w;
    CHECK_EQ(parse_file("meta_long.wav", &w), ERR_OK);
    CHECK_EQ(strlen(w.title), WAV_TEXT_MAX - 1);
    CHECK(strncmp(w.title, "Un titulo larguisimo", 20) == 0);
    CHECK_STR(w.artist, "Art?ista con control");
    CHECK_STR(w.album, "Album");               /* sin espacios al final */

    CHECK_EQ(parse_file("meta_empty_info.wav", &w), ERR_OK);
    CHECK_STR(w.title, "");
    CHECK_EQ(w.flags & WAV_FLAG_META_INFO, 0);

    CHECK_EQ(parse_file("id3_v23.wav", &w), ERR_OK);
    CHECK_STR(w.title, "Titulo ID3");
    CHECK_STR(w.artist, "Artista UTF16");
    CHECK_STR(w.album, "Album BE");
    CHECK(w.flags & WAV_FLAG_META_ID3);

    CHECK_EQ(parse_file("id3_v24_and_info.wav", &w), ERR_OK);
    CHECK_STR(w.title, "Titulo INFO");         /* LIST/INFO tiene prioridad */
    CHECK_STR(w.artist, "Artista ID3");        /* ID3 completa lo que falta */
    CHECK(w.flags & WAV_FLAG_META_INFO);
    CHECK(w.flags & WAV_FLAG_META_ID3);

    CHECK_EQ(parse_file("no_meta.wav", &w), ERR_OK);
    wav_meta_fallback(&w, "/musica/03 - Mi cancion.wav");
    CHECK_STR(w.title, "03 - Mi cancion");
    CHECK_STR(w.artist, "Desconocido");
    CHECK_STR(w.album, "Desconocido");

    wav_info_t z;
    memset(&z, 0, sizeof z);
    wav_meta_fallback(&z, ".wav");
    CHECK_STR(z.title, "Sin titulo");
    memset(&z, 0, sizeof z);
    wav_meta_fallback(&z, "sin_extension");
    CHECK_STR(z.title, "sin_extension");
    memset(&z, 0, sizeof z);
    strcpy(z.title, "Ya tenia");
    wav_meta_fallback(&z, NULL);
    CHECK_STR(z.title, "Ya tenia");
}

static void test_truncated(void)
{
    wav_info_t w;
    CHECK_EQ(parse_file("trunc_data.wav", &w), ERR_OK);
    CHECK(w.flags & WAV_FLAG_TRUNCATED);
    CHECK_EQ(w.data_size % w.block_align, 0);
    CHECK(w.frames < 16000 && w.frames > 7000);
    CHECK_EQ(parse_file("trunc_header.wav", &w), ERR_TRUNCATED);
    CHECK_EQ(parse_file("trunc_tiny.wav", &w), ERR_TRUNCATED);
    CHECK_EQ(parse_file("no_data.wav", &w), ERR_FORMAT);
}

static void test_rejected(void)
{
    wav_info_t w;
    CHECK_EQ(parse_file("pcm8.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("pcm24.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("float32.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("three_ch.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("rate_22k.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("ext_float.wav", &w), ERR_UNSUPPORTED);
    CHECK_EQ(parse_file("not_riff.wav", &w), ERR_FORMAT);
    CHECK_EQ(parse_file("not_wave.wav", &w), ERR_FORMAT);
    CHECK_EQ(parse_file("bad_align.wav", &w), ERR_FORMAT);
    CHECK_EQ(parse_file("fmt_short.wav", &w), ERR_FORMAT);
    CHECK_EQ(wav_parse(NULL, &w), ERR_ARG);
}

/* Todas las longitudes posibles de un archivo valido cortado: nunca debe leer fuera
 * del buffer (los sanitizers lo detectan) ni aceptar algo sin fmt y data completos. */
static void test_every_cut(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/extra_chunks.wav", data_dir);
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (!f)
        return;
    size_t n = fread(file_buf, 1, sizeof file_buf, f);
    fclose(f);
    uint32_t data_off = 12 + 16 + 24 + 12 + 22 + 8;
    int bad = 0;
    for (uint32_t len = 0; len <= 200 && len <= n; ++len) {
        uint8_t *copy = malloc(len ? len : 1);
        memcpy(copy, file_buf, len);
        wav_mem_t m;
        wav_reader_t r;
        wav_info_t w;
        wav_mem_reader(&r, &m, copy, len);
        err_t e = wav_parse(&r, &w);
        if (len < data_off && e != ERR_TRUNCATED)
            bad++;
        if (len >= data_off && e == ERR_OK && w.data_offset + w.data_size > len)
            bad++;
        free(copy);
    }
    CHECK_EQ(bad, 0);
}

static void test_duration_fmt(void)
{
    char b[6];
    CHECK_EQ(wav_duration_s(176400 * 225, 176400), 225);
    CHECK_EQ(wav_duration_s(176399, 176400), 0);
    CHECK_EQ(wav_duration_s(100, 0), 0);
    wav_fmt_mmss(b, 0);      CHECK_STR(b, "00:00");
    wav_fmt_mmss(b, 225);    CHECK_STR(b, "03:45");
    wav_fmt_mmss(b, 5999);   CHECK_STR(b, "99:59");
    wav_fmt_mmss(b, 6000);   CHECK_STR(b, "99:59");
    CHECK_STR(err_str(ERR_UNSUPPORTED), "NO SOPORTADO");
    CHECK_STR(err_str((err_t)42), "DESCONOCIDO");
}

int main(int argc, char **argv)
{
    if (argc > 1)
        data_dir = argv[1];
    RUN(test_le);
    RUN(test_valid_formats);
    RUN(test_chunks);
    RUN(test_metadata);
    RUN(test_truncated);
    RUN(test_rejected);
    RUN(test_every_cut);
    RUN(test_duration_fmt);
    return TEST_REPORT();
}
