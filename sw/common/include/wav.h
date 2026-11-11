/*
 * Parser de archivos WAV (SC-03..SC-06).
 *
 *  - RIFF/WAVE con chunk "fmt " PCM (formato 1, o WAVE_FORMAT_EXTENSIBLE con
 *    subformato PCM), 16 bits, 1 o 2 canales, 8000/16000/44100/48000 Hz.
 *  - Salta los chunks desconocidos respetando el relleno a tamano par.
 *  - Metadatos de LIST/INFO (INAM titulo, IART artista, IPRD album) y, si faltan, del
 *    chunk "id3 " (ID3v2.3/2.4: TIT2, TPE1, TALB; SC-05).
 *  - Duracion = bytes de datos / byte rate (SC-06).
 *
 * El archivo se lee a traves de un lector (wav_reader_t) con acceso por desplazamiento:
 * en el HPS es FatFs (f_lseek + f_read) o un FILE*, en las pruebas un buffer. Asi el
 * parser no necesita el archivo entero en memoria y encuentra un LIST que este despues
 * de los datos sin leerlos.
 *
 * Politica de errores (HP-09):
 *  - Cabecera incompleta (el archivo termina antes de "fmt " o "data"): ERR_TRUNCATED.
 *  - No es RIFF/WAVE o un chunk obligatorio esta mal formado: ERR_FORMAT.
 *  - WAV valido con formato no soportado (8 o 24 bits, 3+ canales, otra frecuencia,
 *    no PCM): ERR_UNSUPPORTED.
 *  - Datos mas cortos que lo declarado: se acepta con lo que hay y se marca
 *    WAV_FLAG_TRUNCATED (se puede reproducir; la duracion es la real).
 */
#ifndef COMMON_WAV_H
#define COMMON_WAV_H

#include <stdint.h>
#include "err.h"

#define WAV_TEXT_MAX 48u                  /* bytes por campo, con el '\0' (P7) */

#define WAV_FLAG_TRUNCATED  (1u << 0)     /* el chunk data declara mas de lo que hay */
#define WAV_FLAG_META_INFO  (1u << 1)     /* algun metadato salio de LIST/INFO */
#define WAV_FLAG_META_ID3   (1u << 2)     /* algun metadato salio de ID3 */
#define WAV_FLAG_EXTENSIBLE (1u << 3)     /* fmt WAVE_FORMAT_EXTENSIBLE */

typedef struct {
    void *ctx;
    uint32_t size;                        /* tamano total del archivo en bytes */
    /* Lee hasta n bytes desde off. Devuelve los bytes leidos (< n si se acaba). */
    uint32_t (*read)(void *ctx, uint32_t off, uint8_t *buf, uint32_t n);
} wav_reader_t;

typedef struct {
    uint16_t channels;                    /* 1 o 2 */
    uint16_t bits;                        /* 16 */
    uint16_t block_align;                 /* bytes por cuadro = channels * 2 */
    uint32_t sample_rate;                 /* Hz */
    uint32_t byte_rate;                   /* sample_rate * block_align */
    uint32_t data_offset;                 /* desplazamiento del primer byte PCM */
    uint32_t data_size;                   /* bytes PCM utiles (multiplo de block_align) */
    uint32_t frames;                      /* data_size / block_align */
    uint32_t duration_s;                  /* data_size / byte_rate */
    uint32_t flags;                       /* WAV_FLAG_* */
    char title[WAV_TEXT_MAX];             /* "" si no hay */
    char artist[WAV_TEXT_MAX];
    char album[WAV_TEXT_MAX];
} wav_info_t;

/* Analiza el archivo. Con ERR_OK, info queda completo (metadatos vacios si no hay). */
err_t wav_parse(const wav_reader_t *r, wav_info_t *info);

/* Valores de respaldo para metadatos que faltan (P7): titulo = nombre del archivo sin
 * ruta ni extension (o "Sin titulo"), artista y album = "Desconocido". */
void wav_meta_fallback(wav_info_t *info, const char *filename);

/* Duracion en segundos de data_size bytes (redondeo hacia abajo). */
uint32_t wav_duration_s(uint32_t data_size, uint32_t byte_rate);

/* "MM:SS" en out (6 bytes); de 100 minutos en adelante satura en "99:59". */
void wav_fmt_mmss(char *out, uint32_t seconds);

/* Lector sobre un buffer en memoria (pruebas del PC y archivos ya cargados). */
typedef struct {
    const uint8_t *buf;
    uint32_t len;
} wav_mem_t;

void wav_mem_reader(wav_reader_t *r, wav_mem_t *m, const uint8_t *buf, uint32_t len);

#endif /* COMMON_WAV_H */
