/*
 * Fuente de audio del Nios V sobre la memoria compartida (NV-13). Ver shm_source.h.
 *
 * Traduce el protocolo de shm_proto.h a la interfaz generica source_port_t que usa el
 * lazo principal: estados SRC_*, tabla de pistas, PLAY/STOP y lectura de cuadros.
 * Todas las funciones reciben ctx = puntero a shm_source_t.
 */
#include "shm_source.h"

/*
 * Estado de la fuente (SRC_*), combinando el estado del HPS y el del flujo:
 *   - sin magic -> WAITING; version distinta, sin SD o sin canciones -> FAILED;
 *   - HPS listo pero sin pista pedida -> IDLE;
 *   - pista pedida sin respuesta todavia -> LOADING;
 *   - con respuesta: RUNNING, ENDED (EOF y ring vacio) o ERROR segun el flujo.
 */
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

/* Cantidad de pistas publicadas por el HPS, acotada a SHM_MAX_TRACKS por si el valor
 * en memoria es basura. */
static uint32_t src_track_count(void *ctx)
{
    shm_source_t *src = (shm_source_t *)ctx;
    uint32_t n = src->s->ctrl.track_count;
    return n > SHM_MAX_TRACKS ? SHM_MAX_TRACKS : n;
}

/* Copia un texto de shm a la RAM local, terminandolo en '\0' aunque en shm no lo este. */
static void copy_text(char *dst, volatile const char *src)
{
    uint32_t i = 0;
    for (; i < SHM_TEXT - 1u && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*
 * Datos de la pista index para la pantalla. Los textos se copian a buffers propios de
 * la fuente (shm podria cambiar mientras se dibujan). Devuelve 0 si la pista existe,
 * -1 si el indice esta fuera de la tabla.
 */
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

/* Pide al HPS la pista index desde el inicio. Se recuerda la secuencia del comando
 * para consumir solo el flujo que responde a este pedido. */
static void src_play(void *ctx, uint32_t index)
{
    shm_source_t *src = (shm_source_t *)ctx;
    src->seq = shm_nios_send(src->s, SHM_CMD_PLAY, index);
    src->active = 1;
}

/* Pide al HPS que detenga el flujo; desde ahora no se consume nada. */
static void src_stop(void *ctx)
{
    shm_source_t *src = (shm_source_t *)ctx;
    src->seq = shm_nios_send(src->s, SHM_CMD_STOP, 0);
    src->active = 0;
}

/* Lee hasta max cuadros del ring en frames. Devuelve los leidos (0 sin pista activa,
 * sin respuesta del HPS o con el ring vacio). */
static uint32_t src_read(void *ctx, uint32_t *frames, uint32_t max)
{
    shm_source_t *src = (shm_source_t *)ctx;
    if (!src->active)
        return 0;
    return shm_nios_read(src->s, src->seq, frames, max);
}

/* Prepara la fuente sobre la memoria compartida s, sin pista activa. */
void shm_source_init(shm_source_t *src, volatile shm_t *s)
{
    src->s = s;
    /* Continua la secuencia que haya en shm: si el Nios V se reinicia sin el HPS, sus
     * comandos nuevos siguen siendo distintos del ultimo ack. */
    src->seq = s->ctrl.cmd_seq;
    src->active = 0;
    src->title[0] = src->artist[0] = src->album[0] = '\0';
}

/* Conecta las funciones de esta fuente al puerto generico que usa app.c. */
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
