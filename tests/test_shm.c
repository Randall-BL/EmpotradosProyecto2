/*
 * Pruebas del protocolo HPS <-> Nios V (SC-13 borrador, NV-13): productor y
 * consumidor sobre la misma estructura, intercalados como en la tarjeta.
 */
#include <stdlib.h>
#include "shm_proto.h"
#include "test.h"

static shm_t shm;

static void test_layout(void)
{
    CHECK_EQ(sizeof(shm_t), SHM_SIZE);
    CHECK_EQ((char *)&shm.tracks[0] - (char *)&shm, SHM_TRACKS_OFF);
    CHECK_EQ((char *)&shm.ring[0] - (char *)&shm, SHM_RING_OFF);
}

static void test_handshake(void)
{
    volatile shm_t *s = &shm;
    uint32_t cmd, arg, seq;
    CHECK_EQ(shm_nios_check(s), ERR_EMPTY);
    shm_hps_init(s);
    CHECK_EQ(shm_nios_check(s), ERR_OK);
    CHECK_EQ(s->ctrl.hps_state, SHM_HPS_BOOTING);
    shm_hps_publish(s, 12);
    CHECK_EQ(s->ctrl.hps_state, SHM_HPS_READY);
    CHECK_EQ(s->ctrl.track_count, 12);

    CHECK_EQ(shm_hps_poll(s, &cmd, &arg, &seq), 0);
    uint32_t q = shm_nios_send(s, SHM_CMD_PLAY, 7);
    CHECK_EQ(shm_nios_acked(s, q), 0);
    CHECK_EQ(shm_nios_stream(s, q), SHM_STREAM_IDLE);
    CHECK_EQ(shm_hps_poll(s, &cmd, &arg, &seq), 1);
    CHECK_EQ(cmd, SHM_CMD_PLAY);
    CHECK_EQ(arg, 7);
    CHECK_EQ(seq, q);
    shm_hps_ack(s, seq, SHM_STREAM_RUNNING);
    CHECK(shm_nios_acked(s, q));
    CHECK_EQ(shm_hps_poll(s, &cmd, &arg, &seq), 0);
    CHECK_EQ(shm_nios_stream(s, q), SHM_STREAM_RUNNING);

    s->ctrl.version = 2;
    CHECK_EQ(shm_nios_check(s), ERR_VERSION);
    shm_hps_publish(s, 0);
    CHECK_EQ(s->ctrl.hps_state, SHM_HPS_NO_TRACKS);
    shm_hps_publish(s, 100);
    CHECK_EQ(s->ctrl.track_count, SHM_MAX_TRACKS);
}

/* Flujo completo con tamanos de bloque irregulares: el consumidor recibe exactamente
 * la secuencia que escribio el productor, sin perder ni repetir cuadros. */
static void test_stream(void)
{
    volatile shm_t *s = &shm;
    shm_hps_init(s);
    shm_hps_publish(s, 1);
    uint32_t q = shm_nios_send(s, SHM_CMD_PLAY, 0);
    uint32_t cmd, arg, seq;
    shm_hps_poll(s, &cmd, &arg, &seq);
    shm_hps_ack(s, seq, SHM_STREAM_RUNNING);

    const uint32_t total = 3u * SHM_RING_FRAMES + 123u;
    uint32_t produced = 0, consumed = 0, errors = 0;
    uint32_t buf[700];
    unsigned seed = 1;
    while (consumed < total) {
        seed = seed * 1103515245u + 12345u;
        uint32_t n = (seed >> 16) % 700u;
        if (produced < total) {
            if (n > total - produced)
                n = total - produced;
            for (uint32_t i = 0; i < n; ++i)
                buf[i] = produced + i;
            produced += shm_hps_write(s, buf, n);
            if (produced == total)
                shm_hps_end(s, SHM_STREAM_EOF);
        }
        CHECK(s->ctrl.wr - s->ctrl.rd <= SHM_RING_FRAMES);
        uint32_t m = shm_nios_read(s, q, buf, (seed >> 8) % 700u);
        for (uint32_t i = 0; i < m; ++i)
            if (buf[i] != consumed + i)
                errors++;
        consumed += m;
        if (consumed < total)
            CHECK(shm_nios_stream(s, q) != SHM_STREAM_EOF);
    }
    CHECK_EQ(errors, 0);
    CHECK_EQ(consumed, total);
    CHECK_EQ(shm_nios_stream(s, q), SHM_STREAM_EOF);
}

/* Cambio de pista con datos viejos en el ring: el Nios V no debe ver nada de la pista
 * anterior, ni antes del ack ni despues. */
static void test_switch_track(void)
{
    volatile shm_t *s = &shm;
    uint32_t cmd, arg, seq, buf[64];
    shm_hps_init(s);
    shm_hps_publish(s, 3);
    uint32_t q1 = shm_nios_send(s, SHM_CMD_PLAY, 0);
    shm_hps_poll(s, &cmd, &arg, &seq);
    shm_hps_ack(s, seq, SHM_STREAM_RUNNING);
    for (uint32_t i = 0; i < 64; ++i)
        buf[i] = 0xAAAA0000u | i;
    shm_hps_write(s, buf, 64);
    CHECK_EQ(shm_nios_read(s, q1, buf, 10), 10);

    uint32_t q2 = shm_nios_send(s, SHM_CMD_PLAY, 2);
    CHECK_EQ(shm_nios_avail(s, q2), 0);        /* pendiente: no consume restos */
    CHECK_EQ(shm_nios_read(s, q2, buf, 64), 0);
    shm_hps_poll(s, &cmd, &arg, &seq);
    CHECK_EQ(arg, 2);
    shm_hps_ack(s, seq, SHM_STREAM_RUNNING);
    CHECK_EQ(shm_nios_avail(s, q2), 0);        /* el ack descarto lo viejo */
    for (uint32_t i = 0; i < 8; ++i)
        buf[i] = 0xBBBB0000u | i;
    shm_hps_write(s, buf, 8);
    CHECK_EQ(shm_nios_read(s, q2, buf, 64), 8);
    CHECK_EQ(buf[0], 0xBBBB0000u);
    /* El flujo viejo (q1) ya no es valido. */
    CHECK_EQ(shm_nios_avail(s, q1), 0);

    /* Error de lectura en el HPS. */
    uint32_t q3 = shm_nios_send(s, SHM_CMD_PLAY, 1);
    shm_hps_poll(s, &cmd, &arg, &seq);
    shm_hps_ack(s, seq, SHM_STREAM_ERROR);
    CHECK_EQ(shm_nios_stream(s, q3), SHM_STREAM_ERROR);

    /* Ring lleno: el productor no sobrescribe. */
    uint32_t q4 = shm_nios_send(s, SHM_CMD_PLAY, 0);
    shm_hps_poll(s, &cmd, &arg, &seq);
    shm_hps_ack(s, seq, SHM_STREAM_RUNNING);
    uint32_t written = 0;
    while (shm_hps_space(s))
        written += shm_hps_write(s, buf, 64);
    CHECK_EQ(written, SHM_RING_FRAMES);
    CHECK_EQ(shm_hps_write(s, buf, 64), 0);
    CHECK_EQ(shm_nios_avail(s, q4), SHM_RING_FRAMES);
}

int main(void)
{
    RUN(test_layout);
    RUN(test_handshake);
    RUN(test_stream);
    RUN(test_switch_track);
    return TEST_REPORT();
}
