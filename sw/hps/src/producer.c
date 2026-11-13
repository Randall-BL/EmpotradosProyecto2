#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE                   /* strcasecmp, DT_* */
#endif
#include "producer.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "le.h"
#include "wav.h"

/* ---- Lector del parser sobre un FILE* ---- */

static uint32_t file_read(void *ctx, uint32_t off, uint8_t *buf, uint32_t n)
{
    FILE *f = ctx;
    if (fseek(f, (long)off, SEEK_SET) != 0)
        return 0;
    return (uint32_t)fread(buf, 1, n, f);
}

static int is_wav(const char *name)
{
    size_t n = strlen(name);
    if (name[0] == '.' || n < 5)          /* ocultos y "._x.wav" de macOS */
        return 0;
    return strcasecmp(name + n - 4, ".wav") == 0;
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void copy_text(char *dst, const char *src)
{
    size_t i = 0;
    for (; i + 1 < SHM_TEXT && src[i]; ++i)
        dst[i] = src[i];
    for (; i < SHM_TEXT; ++i)
        dst[i] = '\0';
}

/* Analiza un archivo. 1 si es reproducible y quedo en file/track. */
static int load_track(const char *path, const char *name, producer_file_t *file,
                      shm_track_t *track)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    wav_info_t info;
    wav_reader_t r = {f, 0, file_read};
    int ok = fseek(f, 0, SEEK_END) == 0;
    long size = ok ? ftell(f) : -1;
    ok = ok && size > 0 && (unsigned long)size <= 0xFFFFFFFFul;
    if (ok) {
        r.size = (uint32_t)size;
        ok = wav_parse(&r, &info) == ERR_OK && info.frames > 0;
    }
    fclose(f);
    if (!ok)
        return 0;

    wav_meta_fallback(&info, name);
    snprintf(file->path, sizeof file->path, "%s", path);
    file->data_offset = info.data_offset;
    file->frames = info.frames;
    file->rate = info.sample_rate;
    file->channels = info.channels;

    memset(track, 0, sizeof *track);
    copy_text(track->title, info.title);
    copy_text(track->artist, info.artist);
    copy_text(track->album, info.album);
    track->duration_s = info.duration_s;
    track->sample_rate = info.sample_rate;
    track->channels = info.channels;
    track->flags = (uint16_t)info.flags;
    track->frames = resample_out_frames(info.frames, info.sample_rate);
    return 1;
}

uint32_t producer_scan(producer_t *p, const char *dir)
{
    p->ntracks = 0;
    p->skipped = 0;
    p->f = NULL;
    DIR *d = opendir(dir);
    if (!d) {
        p->hps_state = SHM_HPS_NO_SD;
        return 0;
    }

    /* Nombres de los .wav, ordenados para que la lista no dependa del orden en FAT. */
    char *names[4 * SHM_MAX_TRACKS];
    uint32_t n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < 4u * SHM_MAX_TRACKS) {
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN)
            continue;
        if (is_wav(e->d_name) && (names[n] = strdup(e->d_name)) != NULL)
            n++;
    }
    closedir(d);
    qsort(names, n, sizeof names[0], cmp_names);

    for (uint32_t i = 0; i < n; ++i) {
        char path[PRODUCER_PATH_MAX];
        int len = snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (p->ntracks < SHM_MAX_TRACKS && len > 0 && (size_t)len < sizeof path &&
            load_track(path, names[i], &p->files[p->ntracks], &p->table[p->ntracks]))
            p->ntracks++;
        else
            p->skipped++;
        free(names[i]);
    }
    p->hps_state = p->ntracks ? SHM_HPS_READY : SHM_HPS_NO_TRACKS;
    return p->ntracks;
}

/* Copia a shm por palabras de 32 bits: el bridge se mapea como memoria de
 * dispositivo, donde un memcpy con accesos desalineados fallaria. */
static void put_track(volatile shm_track_t *dst, const shm_track_t *src)
{
    volatile uint32_t *d = (volatile uint32_t *)dst;
    const uint32_t *w = (const uint32_t *)src;
    for (uint32_t i = 0; i < sizeof(shm_track_t) / 4u; ++i)
        d[i] = w[i];
}

void producer_publish(producer_t *p, volatile shm_t *s)
{
    producer_stop(p);
    p->s = s;
    shm_hps_init(s);
    for (uint32_t i = 0; i < p->ntracks; ++i)
        put_track(&s->tracks[i], &p->table[i]);
    if (p->hps_state == SHM_HPS_READY) {
        shm_hps_publish(s, p->ntracks);
    } else {
        s->ctrl.track_count = 0;
        SHM_BARRIER();
        s->ctrl.hps_state = p->hps_state;
    }
}

void producer_stop(producer_t *p)
{
    if (p->f)
        fclose(p->f);
    p->f = NULL;
    p->in_pos = p->in_len = 0;
}

static uint32_t start_track(producer_t *p, uint32_t track)
{
    producer_stop(p);
    if (track >= p->ntracks)
        return SHM_STREAM_ERROR;
    producer_file_t *file = &p->files[track];
    p->f = fopen(file->path, "rb");
    if (!p->f || fseek(p->f, (long)file->data_offset, SEEK_SET) != 0) {
        producer_stop(p);
        return SHM_STREAM_ERROR;
    }
    p->track = track;
    p->in_left = file->frames;
    resample_init(&p->rs, file->rate);
    return SHM_STREAM_RUNNING;
}

/* Lee el siguiente bloque de PCM. 0 al llegar al final (o error de lectura). */
static int refill(producer_t *p)
{
    producer_file_t *file = &p->files[p->track];
    uint32_t want = p->in_left < PRODUCER_IN_FRAMES ? p->in_left : PRODUCER_IN_FRAMES;
    if (want == 0)
        return 0;
    uint8_t *raw = (uint8_t *)p->in;
    size_t bytes = fread(raw, 1, (size_t)want * file->channels * 2u, p->f);
    uint32_t got = (uint32_t)(bytes / (file->channels * 2u));
    if (got == 0) {
        p->in_left = 0;                   /* archivo mas corto de lo esperado */
        return 0;
    }
    /* El WAV es little-endian: se arma cada muestra por bytes (portable). */
    for (uint32_t i = 0; i < got * file->channels; ++i)
        p->in[i] = (int16_t)le16(&raw[2u * i]);
    p->in_pos = 0;
    p->in_len = got;
    p->in_left -= got;
    return 1;
}

static int stream(producer_t *p)
{
    int did = 0;
    while (p->f) {
        uint32_t space = shm_hps_space(p->s);
        if (space == 0)
            break;
        if (p->in_pos == p->in_len && !refill(p)) {
            producer_stop(p);
            shm_hps_end(p->s, SHM_STREAM_EOF);
            return 1;
        }
        uint32_t room = space < (uint32_t)(sizeof p->out / sizeof p->out[0])
                            ? space : (uint32_t)(sizeof p->out / sizeof p->out[0]);
        uint32_t used;
        uint16_t ch = p->files[p->track].channels;
        uint32_t made = resample_run(&p->rs, &p->in[p->in_pos * ch], p->in_len - p->in_pos,
                                     ch, &used, p->out, room);
        p->in_pos += used;
        if (made)
            shm_hps_write(p->s, p->out, made);
        did = 1;
        if (made == 0 && used == 0)
            break;
    }
    return did;
}

int producer_poll(producer_t *p)
{
    uint32_t cmd, arg, seq;
    int did = 0;
    if (shm_hps_poll(p->s, &cmd, &arg, &seq)) {
        uint32_t st = SHM_STREAM_IDLE;
        if (cmd == SHM_CMD_PLAY)
            st = start_track(p, arg);
        else
            producer_stop(p);
        shm_hps_ack(p->s, seq, st);
        p->commands++;
        did = 1;
    }
    return stream(p) | did;
}
