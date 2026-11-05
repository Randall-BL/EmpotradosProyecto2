#include "shm_proto.h"

#define RING_MASK (SHM_RING_FRAMES - 1u)

/* ===== Nios V ===== */

err_t shm_nios_check(volatile const shm_t *s)
{
    if (s->ctrl.magic != SHM_MAGIC)
        return ERR_EMPTY;
    if (s->ctrl.version != SHM_VERSION)
        return ERR_VERSION;
    return ERR_OK;
}

uint32_t shm_nios_send(volatile shm_t *s, uint32_t cmd, uint32_t arg)
{
    uint32_t seq = s->ctrl.cmd_seq + 1u;
    s->ctrl.cmd = cmd;
    s->ctrl.cmd_arg = arg;
    SHM_BARRIER();
    s->ctrl.cmd_seq = seq;
    return seq;
}

int shm_nios_acked(volatile const shm_t *s, uint32_t seq)
{
    return s->ctrl.ack_seq == seq && s->ctrl.stream_seq == seq;
}

uint32_t shm_nios_avail(volatile const shm_t *s, uint32_t seq)
{
    if (!shm_nios_acked(s, seq))
        return 0;
    uint32_t n = s->ctrl.wr - s->ctrl.rd;
    return n > SHM_RING_FRAMES ? 0u : n;          /* indices incoherentes: nada */
}

uint32_t shm_nios_read(volatile shm_t *s, uint32_t seq, uint32_t *dst, uint32_t max)
{
    uint32_t n = shm_nios_avail(s, seq);
    if (n > max)
        n = max;
    SHM_BARRIER();                                /* leer wr antes que los datos */
    uint32_t rd = s->ctrl.rd;
    for (uint32_t i = 0; i < n; ++i)
        dst[i] = s->ring[(rd + i) & RING_MASK];
    SHM_BARRIER();                                /* datos leidos antes de liberar */
    s->ctrl.rd = rd + n;
    return n;
}

uint32_t shm_nios_stream(volatile const shm_t *s, uint32_t seq)
{
    if (!shm_nios_acked(s, seq))
        return SHM_STREAM_IDLE;
    uint32_t st = s->ctrl.stream_state;
    if (st == SHM_STREAM_EOF && s->ctrl.wr != s->ctrl.rd)
        return SHM_STREAM_RUNNING;                /* quedan cuadros por consumir */
    return st;
}

/* ===== HPS ===== */

void shm_hps_init(volatile shm_t *s)
{
    volatile uint32_t *c = (volatile uint32_t *)&s->ctrl;
    for (uint32_t i = 0; i < sizeof(shm_ctrl_t) / 4u; ++i)
        c[i] = 0;
    s->ctrl.version = SHM_VERSION;
    s->ctrl.hps_state = SHM_HPS_BOOTING;
    SHM_BARRIER();
    s->ctrl.magic = SHM_MAGIC;
}

void shm_hps_publish(volatile shm_t *s, uint32_t track_count)
{
    if (track_count > SHM_MAX_TRACKS)
        track_count = SHM_MAX_TRACKS;
    s->ctrl.track_count = track_count;
    SHM_BARRIER();                                /* tabla visible antes de READY */
    s->ctrl.hps_state = track_count ? SHM_HPS_READY : SHM_HPS_NO_TRACKS;
}

int shm_hps_poll(volatile const shm_t *s, uint32_t *cmd, uint32_t *arg, uint32_t *seq)
{
    uint32_t q = s->ctrl.cmd_seq;
    if (q == s->ctrl.ack_seq)
        return 0;
    SHM_BARRIER();                                /* cmd y arg despues de ver cmd_seq */
    *cmd = s->ctrl.cmd;
    *arg = s->ctrl.cmd_arg;
    *seq = q;
    return 1;
}

void shm_hps_ack(volatile shm_t *s, uint32_t seq, uint32_t stream_state)
{
    s->ctrl.wr = s->ctrl.rd;                      /* descarta lo que quedaba */
    s->ctrl.stream_state = stream_state;
    s->ctrl.stream_seq = seq;
    SHM_BARRIER();
    s->ctrl.ack_seq = seq;
}

uint32_t shm_hps_space(volatile const shm_t *s)
{
    uint32_t used = s->ctrl.wr - s->ctrl.rd;
    return used >= SHM_RING_FRAMES ? 0u : SHM_RING_FRAMES - used;
}

uint32_t shm_hps_write(volatile shm_t *s, const uint32_t *frames, uint32_t n)
{
    uint32_t space = shm_hps_space(s);
    if (n > space)
        n = space;
    uint32_t wr = s->ctrl.wr;
    for (uint32_t i = 0; i < n; ++i)
        s->ring[(wr + i) & RING_MASK] = frames[i];
    SHM_BARRIER();                                /* datos antes del indice */
    s->ctrl.wr = wr + n;
    return n;
}

void shm_hps_end(volatile shm_t *s, uint32_t stream_state)
{
    SHM_BARRIER();
    s->ctrl.stream_state = stream_state;
}
