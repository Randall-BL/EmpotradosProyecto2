#include "shm_source.h"

static uint32_t src_state(void *ctx)
{
    shm_source_t *src = (shm_source_t *)ctx;
    err_t e = shm_nios_check(src->s);
    if (e == ERR_VERSION)
        return SRC_FAILED;
    if (e != ERR_OK)
        return SRC_WAITING;
    switch (src->s->ctrl.hps_state) {
    case SHM_HPS_READY:
        break;
    case SHM_HPS_NO_SD:
    case SHM_HPS_NO_TRACKS:
        return SRC_FAILED;
    default:
        return SRC_WAITING;
    }
    if (!src->active)
        return SRC_IDLE;
    if (!shm_nios_acked(src->s, src->seq))
        return SRC_LOADING;
    switch (shm_nios_stream(src->s, src->seq)) {
    case SHM_STREAM_RUNNING: return SRC_RUNNING;
    case SHM_STREAM_EOF:     return SRC_ENDED;
    case SHM_STREAM_ERROR:   return SRC_ERROR;
    default:                 return SRC_LOADING;
    }
}

static uint32_t src_track_count(void *ctx)
{
    shm_source_t *src = (shm_source_t *)ctx;
    uint32_t n = src->s->ctrl.track_count;
    return n > SHM_MAX_TRACKS ? SHM_MAX_TRACKS : n;
}

static void copy_text(char *dst, volatile const char *src)
{
    uint32_t i = 0;
    for (; i < SHM_TEXT - 1u && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int src_track_meta(void *ctx, uint32_t index, track_meta_t *out)
{
    shm_source_t *src = (shm_source_t *)ctx;
    if (index >= src_track_count(ctx))
        return -1;
    volatile const shm_track_t *t = &src->s->tracks[index];
    copy_text(src->title, t->title);
    copy_text(src->artist, t->artist);
    copy_text(src->album, t->album);
    out->title = src->title;
    out->artist = src->artist;
    out->album = src->album;
    out->duration_s = t->duration_s;
    return 0;
}

static void src_play(void *ctx, uint32_t index)
{
    shm_source_t *src = (shm_source_t *)ctx;
    src->seq = shm_nios_send(src->s, SHM_CMD_PLAY, index);
    src->active = 1;
}

static void src_stop(void *ctx)
{
    shm_source_t *src = (shm_source_t *)ctx;
    src->seq = shm_nios_send(src->s, SHM_CMD_STOP, 0);
    src->active = 0;
}

static uint32_t src_read(void *ctx, uint32_t *frames, uint32_t max)
{
    shm_source_t *src = (shm_source_t *)ctx;
    if (!src->active)
        return 0;
    return shm_nios_read(src->s, src->seq, frames, max);
}

void shm_source_init(shm_source_t *src, volatile shm_t *s)
{
    src->s = s;
    /* Continua la secuencia que haya en shm: si el Nios V se reinicia sin el HPS, sus
     * comandos nuevos siguen siendo distintos del ultimo ack. */
    src->seq = s->ctrl.cmd_seq;
    src->active = 0;
    src->title[0] = src->artist[0] = src->album[0] = '\0';
}

void shm_source_port(shm_source_t *src, source_port_t *port)
{
    port->ctx = src;
    port->state = src_state;
    port->track_count = src_track_count;
    port->track_meta = src_track_meta;
    port->play = src_play;
    port->stop = src_stop;
    port->read = src_read;
}
