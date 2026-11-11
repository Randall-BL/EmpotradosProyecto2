#include "hps_model.h"

static void put_text(volatile char *dst, const char *s, uint32_t n2)
{
    uint32_t i = 0;
    for (; s[i] && i < SHM_TEXT - 4u; ++i)
        dst[i] = s[i];
    if (n2 != ~0u) {                       /* sufijo de dos cifras */
        uint32_t tens = 0;
        while (n2 >= 10u) {
            n2 -= 10u;
            tens++;
        }
        dst[i++] = (char)('0' + tens);
        dst[i++] = (char)('0' + n2);
    }
    dst[i] = '\0';
}

void hps_model_init(hps_model_t *m, volatile shm_t *s, uint32_t ntracks, uint32_t frames,
                    uint32_t bad_mask)
{
    m->s = s;
    m->ntracks = ntracks > SHM_MAX_TRACKS ? SHM_MAX_TRACKS : ntracks;
    m->frames = frames;
    m->bad_mask = bad_mask;
    m->streaming = 0;
    m->track = m->pos = 0;
    m->commands = 0;
    shm_hps_init(s);
    uint32_t dur = 0, left = frames;
    while (left >= 48000u) {
        left -= 48000u;
        dur++;
    }
    for (uint32_t i = 0; i < m->ntracks; ++i) {
        volatile shm_track_t *t = &s->tracks[i];
        put_text(t->title, "Cancion ", i + 1u);
        put_text(t->artist, "Artista de prueba", ~0u);
        put_text(t->album, "Album sintetico", ~0u);
        t->duration_s = dur;
        t->sample_rate = 48000u;
        t->channels = 2u;
        t->flags = 0u;
        t->frames = frames;
    }
    shm_hps_publish(s, m->ntracks);
}

void hps_model_poll(hps_model_t *m, uint32_t max_frames)
{
    uint32_t cmd, arg, seq;
    if (shm_hps_poll(m->s, &cmd, &arg, &seq)) {
        m->commands++;
        if (cmd == SHM_CMD_PLAY && arg < m->ntracks && !(m->bad_mask & (1u << arg))) {
            m->track = arg;
            m->pos = 0;
            m->streaming = 1;
            shm_hps_ack(m->s, seq, SHM_STREAM_RUNNING);
        } else {
            m->streaming = 0;
            shm_hps_ack(m->s, seq, cmd == SHM_CMD_PLAY ? SHM_STREAM_ERROR : SHM_STREAM_IDLE);
        }
    }
    if (!m->streaming)
        return;
    uint32_t buf[64];
    while (max_frames && m->pos < m->frames) {
        uint32_t n = m->frames - m->pos;
        if (n > max_frames)
            n = max_frames;
        if (n > 64u)
            n = 64u;
        for (uint32_t i = 0; i < n; ++i)
            buf[i] = hps_model_frame(m->track, m->pos + i);
        uint32_t w = shm_hps_write(m->s, buf, n);
        m->pos += w;
        max_frames -= w;
        if (w < n)
            break;                         /* ring lleno */
    }
    if (m->pos >= m->frames) {
        shm_hps_end(m->s, SHM_STREAM_EOF);
        m->streaming = 0;
    }
}
