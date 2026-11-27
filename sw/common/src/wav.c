/*
 * Parser de archivos WAV (SC-03..SC-06). Ver wav.h para el formato soportado y la
 * politica de errores.
 *
 * Estructura de un WAV (RIFF):
 *   "RIFF" <tamano 32 bits> "WAVE"            cabecera de 12 bytes
 *   <id 4 bytes> <tamano 32 bits> <cuerpo>    chunks, uno detras de otro
 * Chunks que interesan: "fmt " (formato), "data" (muestras PCM), "LIST"/"INFO"
 * (metadatos) e "id3 " (etiqueta ID3v2 incrustada). Un chunk de tamano impar lleva un
 * byte de relleno. Todos los enteros son little-endian salvo los de ID3 (big-endian).
 *
 * El archivo nunca se carga entero: se lee por desplazamientos con wav_reader_t y
 * cada lectura se valida contra el tamano real del archivo.
 */
#include "wav.h"
#include "le.h"

/* Valores del campo wFormatTag del chunk "fmt ". */
#define WAV_FORMAT_PCM        1u          /* PCM entero sin compresion */
#define WAV_FORMAT_EXTENSIBLE 0xFFFEu     /* formato real en el GUID SubFormat */

/* ---------- lectura acotada ---------- */

/* Lee exactamente n bytes desde off. ERR_TRUNCATED si el rango se sale del archivo,
 * ERR_IO si el lector entrego menos bytes de los pedidos. */
static err_t read_exact(const wav_reader_t *r, uint32_t off, uint8_t *buf, uint32_t n)
{
    if (off > r->size || n > r->size - off)
        return ERR_TRUNCATED;
    return r->read(r->ctx, off, buf, n) == n ? ERR_OK : ERR_IO;
}

/* ---------- texto de metadatos ---------- */

/* Copia un texto de largo len a dst (WAV_TEXT_MAX), cortando en el primer '\0', con
 * los caracteres no imprimibles cambiados por '?' y sin espacios al final. */
static void text_copy(char *dst, const uint8_t *src, uint32_t len)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < len && n < WAV_TEXT_MAX - 1u; ++i) {
        uint8_t c = src[i];
        if (c == 0)
            break;
        dst[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    while (n > 0 && dst[n - 1] == ' ')
        n--;
    dst[n] = '\0';
}

/* Lee un campo de texto del archivo (solo los primeros WAV_TEXT_MAX-1 bytes) y lo
 * deja limpio en dst con text_copy(). */
static err_t text_read(const wav_reader_t *r, uint32_t off, uint32_t len, char *dst)
{
    uint8_t tmp[WAV_TEXT_MAX];
    if (len > WAV_TEXT_MAX - 1u)
        len = WAV_TEXT_MAX - 1u;
    err_t e = read_exact(r, off, tmp, len);
    if (e != ERR_OK)
        return e;
    text_copy(dst, tmp, len);
    return ERR_OK;
}

/* ---------- LIST/INFO (SC-04) ---------- */

/*
 * Recorre los subchunks de un LIST/INFO entre off y end. Cada subchunk es
 * <id 4> <tamano 4> <texto>; se guardan INAM (titulo), IART (artista) e IPRD (album)
 * y se ignoran los demas. Un subchunk mal formado corta el recorrido sin error: los
 * metadatos son opcionales.
 */
static void parse_info(const wav_reader_t *r, uint32_t off, uint32_t end, wav_info_t *info)
{
    uint8_t h[8];
    while (end - off >= 8u) {
        if (read_exact(r, off, h, 8) != ERR_OK)
            return;
        uint32_t sz = le32(h + 4);
        uint32_t body = off + 8u;
        if (sz > end - body)
            return;                                   /* subchunk mal formado: se ignora */
        char *dst = 0;
        if (le_fourcc_eq(h, "INAM"))
            dst = info->title;
        else if (le_fourcc_eq(h, "IART"))
            dst = info->artist;
        else if (le_fourcc_eq(h, "IPRD"))
            dst = info->album;
        if (dst && text_read(r, body, sz, dst) == ERR_OK && dst[0])
            info->flags |= WAV_FLAG_META_INFO;
        /* Siguiente subchunk, respetando el relleno a tamano par. */
        uint32_t step = sz + (sz & 1u);
        if (step > end - body)
            return;
        off = body + step;
    }
}

/* ---------- ID3v2 dentro del WAV (SC-05) ---------- */

/* Entero "syncsafe" de ID3v2: 4 bytes de los que solo se usan los 7 bits bajos
 * (28 bits en total). Asi el valor nunca contiene un byte 0xFF. */
static uint32_t syncsafe32(const uint8_t *p)
{
    return ((uint32_t)(p[0] & 0x7Fu) << 21) | ((uint32_t)(p[1] & 0x7Fu) << 14) |
           ((uint32_t)(p[2] & 0x7Fu) << 7) | (uint32_t)(p[3] & 0x7Fu);
}

/* Entero de 32 bits big-endian (tamano de frame en ID3v2.3). */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* Texto de un frame T***: byte de codificacion + texto. ISO-8859-1 y UTF-8 se copian
 * (lo que no es ASCII queda '?'); en UTF-16 se toman las unidades ASCII. */
static void id3_text(const wav_reader_t *r, uint32_t off, uint32_t len, char *dst)
{
    uint8_t tmp[2u * WAV_TEXT_MAX + 3u];
    if (len < 2u || dst[0])                           /* vacio, o ya vino de LIST/INFO */
        return;
    if (len > sizeof tmp)
        len = sizeof tmp;
    if (read_exact(r, off, tmp, len) != ERR_OK)
        return;
    /* Codificacion: 0 = ISO-8859-1, 1 = UTF-16 con BOM, 2 = UTF-16BE, 3 = UTF-8. */
    uint8_t enc = tmp[0];
    if (enc == 0u || enc == 3u) {
        text_copy(dst, tmp + 1, len - 1u);
        return;
    }
    if (enc != 1u && enc != 2u)
        return;
    uint32_t i = 1, n = 0;
    int big = (enc == 2u);
    if (enc == 1u && len >= 3u) {                     /* BOM */
        big = (tmp[1] == 0xFEu && tmp[2] == 0xFFu);
        i = 3;
    }
    /* UTF-16: se arma cada unidad de 16 bits segun el orden de bytes y se conservan
     * solo las ASCII; el resto queda '?'. */
    uint8_t ascii[WAV_TEXT_MAX];
    for (; i + 1u < len && n < WAV_TEXT_MAX - 1u; i += 2u) {
        uint32_t cu = big ? ((uint32_t)tmp[i] << 8) | tmp[i + 1u]
                          : ((uint32_t)tmp[i + 1u] << 8) | tmp[i];
        if (cu == 0)
            break;
        ascii[n++] = (uint8_t)(cu < 0x80u ? cu : '?');
    }
    text_copy(dst, ascii, n);
}

/*
 * Lee una etiqueta ID3v2.3 o 2.4 guardada en un chunk "id3 " entre off y end.
 * Cabecera de 10 bytes: "ID3", version, revision, banderas, tamano syncsafe. Despues
 * vienen frames de 10 bytes de cabecera (<id 4> <tamano 4> <banderas 2>) + cuerpo.
 * Se toman TIT2 (titulo), TPE1 (artista) y TALB (album), solo para los campos que no
 * hayan venido ya de LIST/INFO. Si algun campo se lleno, se marca WAV_FLAG_META_ID3.
 */
static void parse_id3(const wav_reader_t *r, uint32_t off, uint32_t end, wav_info_t *info)
{
    uint8_t h[10];
    if (end - off < 10u || read_exact(r, off, h, 10) != ERR_OK)
        return;
    if (h[0] != 'I' || h[1] != 'D' || h[2] != '3' || (h[3] != 3u && h[3] != 4u))
        return;
    if (h[5] & 0x80u)                                 /* unsynchronisation: no soportado */
        return;
    uint32_t v4 = (h[3] == 4u);                       /* en 2.4 el tamano de frame es syncsafe */
    uint32_t tag_end = off + 10u;
    uint32_t tag_size = syncsafe32(h + 6);
    if (tag_size > end - tag_end)
        tag_size = end - tag_end;
    tag_end += tag_size;
    off += 10u;
    if (h[5] & 0x40u) {                               /* encabezado extendido */
        uint8_t x[4];
        if (tag_end - off < 4u || read_exact(r, off, x, 4) != ERR_OK)
            return;
        uint32_t xs = v4 ? syncsafe32(x) : be32(x) + 4u;
        if (xs > tag_end - off)
            return;
        off += xs;
    }
    /* Se recuerda que campos estaban vacios para saber si ID3 aporto alguno. */
    char before_t = info->title[0], before_a = info->artist[0], before_b = info->album[0];
    while (tag_end - off >= 10u) {
        if (read_exact(r, off, h, 10) != ERR_OK || h[0] == 0)
            break;                                    /* relleno */
        uint32_t sz = v4 ? syncsafe32(h + 4) : be32(h + 4);
        uint32_t body = off + 10u;
        if (sz > tag_end - body)
            break;
        if (le_fourcc_eq(h, "TIT2"))
            id3_text(r, body, sz, info->title);
        else if (le_fourcc_eq(h, "TPE1"))
            id3_text(r, body, sz, info->artist);
        else if (le_fourcc_eq(h, "TALB"))
            id3_text(r, body, sz, info->album);
        off = body + sz;
    }
    if ((!before_t && info->title[0]) || (!before_a && info->artist[0]) ||
        (!before_b && info->album[0]))
        info->flags |= WAV_FLAG_META_ID3;
}

/* ---------- fmt ---------- */

/*
 * Lee el chunk "fmt " (sz bytes desde body). Campos (little-endian):
 *   +0 formato (2)  +2 canales (2)  +4 frecuencia (4)  +8 bytes por segundo (4)
 *   +12 bytes por cuadro (2)  +14 bits por muestra (2)  [+16 extension...]
 * Con WAVE_FORMAT_EXTENSIBLE el formato real esta en el GUID SubFormat. Devuelve
 * ERR_UNSUPPORTED si no es PCM; los demas campos se validan en check_fmt().
 */
static err_t parse_fmt(const wav_reader_t *r, uint32_t body, uint32_t sz, wav_info_t *info)
{
    uint8_t f[40];
    if (sz < 16u)
        return ERR_FORMAT;
    uint32_t n = sz < sizeof f ? sz : (uint32_t)sizeof f;
    err_t e = read_exact(r, body, f, n);
    if (e != ERR_OK)
        return e;
    uint32_t tag = le16(f);
    info->channels = le16(f + 2);
    info->sample_rate = le32(f + 4);
    info->byte_rate = le32(f + 8);
    info->block_align = le16(f + 12);
    info->bits = le16(f + 14);
    if (tag == WAV_FORMAT_EXTENSIBLE) {
        /* cbSize(2) validBits(2) channelMask(4) SubFormat GUID(16): los 2 primeros
         * bytes del GUID son el formato real. */
        if (n < 40u)
            return ERR_FORMAT;
        tag = le16(f + 24);
        info->flags |= WAV_FLAG_EXTENSIBLE;
    }
    if (tag != WAV_FORMAT_PCM)
        return ERR_UNSUPPORTED;
    return ERR_OK;
}

/*
 * Verifica que el formato sea uno de los soportados (16 bits, 1-2 canales, 8/16/44.1/
 * 48 kHz) y que los campos derivados sean coherentes (block_align = canales * 2 y
 * byte_rate = frecuencia * block_align). Formato no soportado -> ERR_UNSUPPORTED;
 * campos incoherentes -> ERR_FORMAT.
 */
static err_t check_fmt(const wav_info_t *info)
{
    if (info->bits != 16u)
        return ERR_UNSUPPORTED;
    if (info->channels < 1u || info->channels > 2u)
        return ERR_UNSUPPORTED;
    switch (info->sample_rate) {
    case 8000u: case 16000u: case 44100u: case 48000u:
        break;
    default:
        return ERR_UNSUPPORTED;
    }
    if (info->block_align != info->channels * 2u)
        return ERR_FORMAT;
    if (info->byte_rate != info->sample_rate * info->block_align)
        return ERR_FORMAT;
    return ERR_OK;
}

/* ---------- RIFF ---------- */

/* Pone toda la estructura en cero (sin memset, para no depender de la libc). */
static void info_clear(wav_info_t *info)
{
    uint8_t *p = (uint8_t *)info;
    for (uint32_t i = 0; i < sizeof *info; ++i)
        p[i] = 0;
}

/*
 * Analiza el archivo completo:
 *   1. Valida la cabecera RIFF/WAVE y calcula el fin real (lo que declara RIFF o el
 *      tamano del archivo, lo que sea menor).
 *   2. Recorre los chunks: toma el primer "fmt " y el primer "data", y lee metadatos
 *      de LIST/INFO e id3; los demas chunks se saltan.
 *   3. Decide el error si falta fmt o data (truncado o mal formado).
 *   4. Valida el formato y calcula cuadros y duracion con cuadros completos.
 */
err_t wav_parse(const wav_reader_t *r, wav_info_t *info)
{
    if (!r || !r->read || !info)
        return ERR_ARG;
    info_clear(info);

    uint8_t h[12];
    err_t e = read_exact(r, 0, h, 12);
    if (e != ERR_OK)
        return e;
    if (!le_fourcc_eq(h, "RIFF") || !le_fourcc_eq(h + 8, "WAVE"))
        return ERR_FORMAT;
    /* El fin es el menor entre lo que declara RIFF y el tamano real del archivo. */
    uint32_t riff = le32(h + 4);
    if (riff < 4u)
        return ERR_FORMAT;
    uint32_t end = r->size;
    int short_file = riff > r->size - 8u;             /* declara mas de lo que hay */
    if (!short_file)
        end = riff + 8u;

    /* cut = algun chunk declara mas bytes de los que quedan en el archivo. */
    int have_fmt = 0, have_data = 0, cut = 0;
    err_t fmt_err = ERR_OK;
    uint32_t off = 12;
    while (end - off >= 8u) {
        e = read_exact(r, off, h, 8);
        if (e != ERR_OK)
            return e;
        uint32_t sz = le32(h + 4);
        uint32_t body = off + 8u;
        uint32_t avail = end - body;              /* bytes que quedan despues de la cabecera */

        if (le_fourcc_eq(h, "data")) {
            /* data puede venir cortado: se acepta con lo que haya (WAV_FLAG_TRUNCATED). */
            if (!have_data) {
                have_data = 1;
                info->data_offset = body;
                info->data_size = sz;
                if (sz > avail) {
                    info->data_size = avail;
                    info->flags |= WAV_FLAG_TRUNCATED;
                }
            }
        } else if (sz > avail) {
            cut = 1;                                  /* chunk que se sale del archivo */
            break;
        } else if (le_fourcc_eq(h, "fmt ")) {
            if (!have_fmt) {
                have_fmt = 1;
                fmt_err = parse_fmt(r, body, sz, info);
            }
        } else if (le_fourcc_eq(h, "LIST")) {
            uint8_t t[4];
            if (sz >= 4u && read_exact(r, body, t, 4) == ERR_OK && le_fourcc_eq(t, "INFO"))
                parse_info(r, body + 4u, body + sz, info);
        } else if (le_fourcc_eq(h, "id3 ") || le_fourcc_eq(h, "ID3 ")) {
            parse_id3(r, body, body + sz, info);
        }
        /* Relleno a tamano par (RIFF): un chunk impar lleva un byte extra. */
        if (sz > avail || (sz & 1u && sz + 1u > avail))
            break;
        off = body + sz + (sz & 1u);
    }
    /* Falta fmt o data: si el archivo se corto es TRUNCATED, si no, esta mal formado. */
    if (!have_fmt || !have_data)
        return (cut || short_file || end - off != 0u) ? ERR_TRUNCATED : ERR_FORMAT;
    if (fmt_err != ERR_OK)
        return fmt_err;
    e = check_fmt(info);
    if (e != ERR_OK)
        return e;

    /* Solo cuadros completos: un archivo cortado a la mitad de un cuadro desalinearia
     * los canales. */
    uint32_t rem = info->data_size % info->block_align;
    if (rem) {
        info->data_size -= rem;
        info->flags |= WAV_FLAG_TRUNCATED;
    }
    info->frames = info->data_size / info->block_align;
    info->duration_s = wav_duration_s(info->data_size, info->byte_rate);
    return ERR_OK;
}

/* ---------- respaldo y formato ---------- */

/* Copia una cadena fija a un campo de WAV_TEXT_MAX bytes. */
static void str_set(char *dst, const char *src)
{
    uint32_t i = 0;
    for (; src[i] && i < WAV_TEXT_MAX - 1u; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

/* Completa los metadatos vacios: titulo = nombre del archivo sin ruta ni extension
 * (o "Sin titulo"), artista y album = "Desconocido". */
void wav_meta_fallback(wav_info_t *info, const char *filename)
{
    if (!info)
        return;
    if (!info->title[0]) {
        /* Nombre del archivo sin ruta: se avanza base despues de cada '/' o '\\'. */
        const char *base = filename ? filename : "";
        for (const char *p = base; *p; ++p)
            if (*p == '/' || *p == '\\')
                base = p + 1;
        /* stem = largo del nombre sin la extension (hasta el ultimo punto). */
        uint32_t len = 0, stem = 0;
        int has_dot = 0;
        for (; base[len]; ++len)
            if (base[len] == '.') {
                stem = len;                           /* ultimo punto: la extension */
                has_dot = 1;
            }
        if (!has_dot)
            stem = len;
        text_copy(info->title, (const uint8_t *)base, stem);
        if (!info->title[0])
            str_set(info->title, "Sin titulo");
    }
    if (!info->artist[0])
        str_set(info->artist, "Desconocido");
    if (!info->album[0])
        str_set(info->album, "Desconocido");
}

/* data_size / byte_rate (0 si byte_rate es 0). Solo se usa en el HPS y en el PC, que
 * tienen divisor. */
uint32_t wav_duration_s(uint32_t data_size, uint32_t byte_rate)
{
    return byte_rate ? data_size / byte_rate : 0u;
}

/* "MM:SS" con division normal (version para el HPS/PC; el Nios V usa screen_fmt_mmss). */
void wav_fmt_mmss(char *out, uint32_t seconds)
{
    uint32_t m = seconds / 60u, s = seconds % 60u;
    if (m > 99u) {
        m = 99u;
        s = 59u;
    }
    out[0] = (char)('0' + m / 10u);
    out[1] = (char)('0' + m % 10u);
    out[2] = ':';
    out[3] = (char)('0' + s / 10u);
    out[4] = (char)('0' + s % 10u);
    out[5] = '\0';
}

/* Funcion read del lector en memoria: copia hasta n bytes desde off del buffer y
 * devuelve cuantos copio (menos si se acaba el buffer). */
static uint32_t mem_read(void *ctx, uint32_t off, uint8_t *buf, uint32_t n)
{
    const wav_mem_t *m = (const wav_mem_t *)ctx;
    if (off >= m->len)
        return 0;
    if (n > m->len - off)
        n = m->len - off;
    for (uint32_t i = 0; i < n; ++i)
        buf[i] = m->buf[off + i];
    return n;
}

/* Arma un lector wav_reader_t sobre un buffer en memoria (m guarda el buffer). */
void wav_mem_reader(wav_reader_t *r, wav_mem_t *m, const uint8_t *buf, uint32_t len)
{
    m->buf = buf;
    m->len = len;
    r->ctx = m;
    r->size = len;
    r->read = mem_read;
}
