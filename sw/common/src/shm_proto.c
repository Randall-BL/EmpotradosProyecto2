/*
 * Protocolo HPS <-> Nios V sobre la memoria compartida (SC-13). Ver shm_proto.h para
 * el mapa de memoria, quien escribe cada campo y la secuencia de comandos.
 *
 * Regla general: cada lado escribe primero los datos y despues, tras una barrera,
 * el indice o la marca que los publica. El otro lado lee primero la marca y despues,
 * tras otra barrera, los datos. Asi nunca se lee un dato a medio escribir.
 */
#include "shm_proto.h"

/* Mascara para pasar de indice libre a posicion dentro del ring. */
#define RING_MASK (SHM_RING_FRAMES - 1u)

/* ===== Nios V ===== */

/*
 * Revisa la cabecera que escribe el HPS: ERR_EMPTY si todavia no hay magic (HPS
 * arrancando o memoria sin inicializar), ERR_VERSION si el protocolo no coincide, y
 * ERR_OK si se puede usar.
 */
err_t shm_nios_check(volatile const shm_t *s)
{
    if (s->ctrl.magic != SHM_MAGIC)
        return ERR_EMPTY;
    if (s->ctrl.version != SHM_VERSION)
        return ERR_VERSION;
    return ERR_OK;
}

/*
 * Envia un comando al HPS: escribe cmd y su argumento, barrera, y por ultimo incrementa
 * cmd_seq (que es lo que el HPS mira para saber que hay un comando nuevo). Devuelve el
 * numero de secuencia asignado, para reconocer despues la respuesta.
 */
uint32_t shm_nios_send(volatile shm_t *s, uint32_t cmd, uint32_t arg)
{
    uint32_t seq = s->ctrl.cmd_seq + 1u;
    s->ctrl.cmd = cmd;
    s->ctrl.cmd_arg = arg;
    SHM_BARRIER();
    s->ctrl.cmd_seq = seq;
    return seq;
}

/* 1 si el HPS ya respondio el comando seq y el flujo del ring es el de ese comando. */
int shm_nios_acked(volatile const shm_t *s, uint32_t seq)
{
    return s->ctrl.ack_seq == seq && s->ctrl.stream_seq == seq;
}

/* Cuadros listos para leer del flujo seq. 0 si el comando todavia no se atendio o si
 * los indices no tienen sentido (wr - rd mayor que el ring). */
uint32_t shm_nios_avail(volatile const shm_t *s, uint32_t seq)
{
    if (!shm_nios_acked(s, seq))
        return 0;
    uint32_t n = s->ctrl.wr - s->ctrl.rd;
    return n > SHM_RING_FRAMES ? 0u : n;          /* indices incoherentes: nada */
}

/*
 * Copia hasta max cuadros del ring a dst y avanza rd (libera ese espacio para el
 * HPS). Devuelve cuantos copio (0 si no hay o el flujo no es el de seq).
 */
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

/*
 * Estado del flujo seq visto desde el consumidor. El HPS marca EOF apenas escribe el
 * ultimo cuadro, pero para el Nios V la pista solo termina cuando ademas consumio
 * todo el ring: mientras queden cuadros se informa RUNNING.
 */
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

/*
 * Inicializa el bloque de control: todo en cero, version y estado BOOTING, y al final
 * (tras una barrera) el magic. El Nios V no usa shm hasta ver el magic, asi que nunca
 * ve un control a medio inicializar.
 */
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

/*
 * Publica la tabla de pistas que ya se escribio en s->tracks: fija la cantidad y pasa
 * a READY (o a NO_TRACKS si no hay ninguna). La barrera garantiza que el Nios V vea
 * la tabla completa cuando ve READY.
 */
void shm_hps_publish(volatile shm_t *s, uint32_t track_count)
{
    if (track_count > SHM_MAX_TRACKS)
        track_count = SHM_MAX_TRACKS;
    s->ctrl.track_count = track_count;
    SHM_BARRIER();                                /* tabla visible antes de READY */
    s->ctrl.hps_state = track_count ? SHM_HPS_READY : SHM_HPS_NO_TRACKS;
}

/*
 * Revisa si el Nios V envio un comando que todavia no se respondio (cmd_seq distinto
 * de ack_seq). Si lo hay devuelve 1 y lo entrega en cmd, arg y seq; si no, 0.
 */
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

/*
 * Responde el comando seq: descarta lo que quedaba en el ring (wr = rd), fija el
 * estado del flujo nuevo y su numero, y por ultimo publica ack_seq. Desde ese momento
 * el Nios V consume solo cuadros del flujo nuevo.
 */
void shm_hps_ack(volatile shm_t *s, uint32_t seq, uint32_t stream_state)
{
    s->ctrl.wr = s->ctrl.rd;                      /* descarta lo que quedaba */
    s->ctrl.stream_state = stream_state;
    s->ctrl.stream_seq = seq;
    SHM_BARRIER();
    s->ctrl.ack_seq = seq;
}

/* Cuadros que todavia caben en el ring (0 si esta lleno o los indices no cuadran). */
uint32_t shm_hps_space(volatile const shm_t *s)
{
    uint32_t used = s->ctrl.wr - s->ctrl.rd;
    return used >= SHM_RING_FRAMES ? 0u : SHM_RING_FRAMES - used;
}

/*
 * Escribe hasta n cuadros en el ring (lo que quepa) y despues, tras una barrera,
 * avanza wr para publicarlos. Devuelve cuantos escribio.
 */
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

/* Marca el final del flujo (EOF o ERROR). La barrera asegura que el ultimo wr ya se
 * vea antes que la marca de fin. */
void shm_hps_end(volatile shm_t *s, uint32_t stream_state)
{
    SHM_BARRIER();
    s->ctrl.stream_state = stream_state;
}
