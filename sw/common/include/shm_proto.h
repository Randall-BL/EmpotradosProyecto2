/*
 * Protocolo HPS <-> Nios V sobre la memoria compartida `shm` (SC-13, BORRADOR).
 *
 * SC-13 es de la persona D. Este borrador lo escribe C porque el lado del Nios V
 * (NV-13) y el lazo principal (NV-14) lo necesitan; D lo revisa y lo ajusta cuando
 * escriba el productor del HPS (HP-06, HP-07). Los dos procesadores son de 32 bits
 * little-endian, asi que la estructura se comparte tal cual (_Static_assert abajo).
 *
 * Mapa de los 64 KB de shm (Nios V: SHM_BASE = 0x0002_0000; HPS: bridge liviano):
 *
 *   0x0000  shm_ctrl_t     64 B    control: estado, comandos e indices del ring
 *   0x0040  shm_track_t[]  5 KB    tabla de pistas (SHM_MAX_TRACKS x 160 B)
 *   0x8000  ring           32 KB   SHM_RING_FRAMES cuadros estereo de 32 bits
 *
 * Cada campo lo escribe UN solo lado (sin exclusion mutua, CD-06):
 *   HPS:    magic, version, hps_state, track_count, tabla, ack_seq, stream_seq,
 *           stream_state, wr y los cuadros del ring.
 *   Nios V: cmd, cmd_arg, cmd_seq, rd, underruns.
 *
 * Ring: indices de cuadro que corren libres (uint32_t); el cuadro i esta en
 * ring[i & (SHM_RING_FRAMES-1)]. Lleno: wr - rd == SHM_RING_FRAMES. Vacio: wr == rd.
 * Cada cuadro es estereo a 48 kHz (CD-07: el HPS remuestrea y duplica el mono),
 * empaquetado como filtro_pack(): izquierdo en los 16 bits bajos, derecho en los altos.
 *
 * Comandos: el Nios V escribe cmd y cmd_arg, barrera, y cmd_seq + 1. El HPS, al verlo
 * (cmd_seq != ack_seq): para el flujo anterior, descarta el ring (wr = rd: el Nios V
 * no consume mientras espera el ack), pone stream_seq = cmd_seq y stream_state, y por
 * ultimo ack_seq = cmd_seq. El Nios V solo consume cuando ack_seq == stream_seq ==
 * el ultimo cmd_seq que envio: asi no reproduce restos de la pista anterior.
 * Fin de pista: el HPS escribe el ultimo cuadro y despues stream_state = EOF; el Nios V
 * la da por terminada cuando ve EOF y el ring vacio.
 */
#ifndef COMMON_SHM_PROTO_H
#define COMMON_SHM_PROTO_H

#include <stdint.h>
#include "err.h"

#define SHM_MAGIC        0x31504853u      /* "SHP1" */
#define SHM_VERSION      1u
#define SHM_SIZE         0x10000u
#define SHM_MAX_TRACKS   32u
#define SHM_TEXT         48u              /* = WAV_TEXT_MAX */
#define SHM_TRACKS_OFF   0x0040u
#define SHM_RING_OFF     0x8000u
#define SHM_RING_FRAMES  8192u            /* 170 ms a 48 kHz; potencia de 2 */

/* hps_state */
#define SHM_HPS_BOOTING  0u               /* memoria sin inicializar o arrancando */
#define SHM_HPS_READY    1u               /* tabla de pistas publicada */
#define SHM_HPS_NO_SD    2u               /* sin SD o sin FAT32 (HP-09) */
#define SHM_HPS_NO_TRACKS 3u              /* SD sin WAV validos */

/* cmd (Nios V -> HPS) */
#define SHM_CMD_NONE     0u
#define SHM_CMD_PLAY     1u               /* cmd_arg = indice de pista */
#define SHM_CMD_STOP     2u

/* stream_state (HPS -> Nios V) */
#define SHM_STREAM_IDLE    0u
#define SHM_STREAM_RUNNING 1u
#define SHM_STREAM_EOF     2u             /* ya se escribio el ultimo cuadro */
#define SHM_STREAM_ERROR   3u             /* la pista no se pudo leer: se salta */

typedef struct {
    /* ---- escribe el HPS ---- */
    uint32_t magic;
    uint32_t version;
    uint32_t hps_state;
    uint32_t track_count;
    uint32_t ack_seq;
    uint32_t stream_seq;
    uint32_t stream_state;
    uint32_t wr;
    /* ---- escribe el Nios V ---- */
    uint32_t cmd;
    uint32_t cmd_arg;
    uint32_t cmd_seq;
    uint32_t rd;
    uint32_t underruns;
    uint32_t reserved[3];
} shm_ctrl_t;

typedef struct {
    char title[SHM_TEXT];
    char artist[SHM_TEXT];
    char album[SHM_TEXT];
    uint32_t duration_s;
    uint32_t sample_rate;                 /* del archivo (el ring siempre va a 48 kHz) */
    uint16_t channels;
    uint16_t flags;                       /* WAV_FLAG_* */
    uint32_t frames;                      /* cuadros a 48 kHz que se van a entregar */
} shm_track_t;

typedef struct {
    shm_ctrl_t ctrl;
    shm_track_t tracks[SHM_MAX_TRACKS];
    uint8_t pad[SHM_RING_OFF - sizeof(shm_ctrl_t) - SHM_MAX_TRACKS * sizeof(shm_track_t)];
    uint32_t ring[SHM_RING_FRAMES];
} shm_t;

_Static_assert(sizeof(shm_ctrl_t) == 64u, "shm_ctrl_t debe medir 64 B");
_Static_assert(sizeof(shm_track_t) == 160u, "shm_track_t debe medir 160 B");
_Static_assert(sizeof(shm_t) == SHM_SIZE, "shm_t debe ocupar los 64 KB de shm");
_Static_assert((SHM_RING_FRAMES & (SHM_RING_FRAMES - 1u)) == 0u, "ring: potencia de 2");

/* Barrera de memoria: los datos deben quedar escritos antes de publicar un indice.
 * En el Nios V/m (RV32I) es un "fence"; en el ARM un "dmb". */
#define SHM_BARRIER() __sync_synchronize()

/* ===== Lado Nios V (consumidor, NV-13) ===== */

/* ERR_OK si el HPS inicializo shm con este protocolo; ERR_EMPTY si todavia no;
 * ERR_VERSION si la version difiere. */
err_t shm_nios_check(volatile const shm_t *s);

/* Envia un comando y devuelve su numero de secuencia. */
uint32_t shm_nios_send(volatile shm_t *s, uint32_t cmd, uint32_t arg);

/* 1 si el HPS ya atendio el comando seq y el ring trae su flujo. */
int shm_nios_acked(volatile const shm_t *s, uint32_t seq);

/* Cuadros disponibles del flujo seq (0 si todavia no se atendio). */
uint32_t shm_nios_avail(volatile const shm_t *s, uint32_t seq);

/* Copia hasta max cuadros del ring a dst y libera su espacio. Devuelve los copiados. */
uint32_t shm_nios_read(volatile shm_t *s, uint32_t seq, uint32_t *dst, uint32_t max);

/* Estado del flujo seq: SHM_STREAM_*; EOF solo cuando ademas el ring quedo vacio. */
uint32_t shm_nios_stream(volatile const shm_t *s, uint32_t seq);

/* ===== Lado HPS (productor, HP-06/HP-07; tambien lo usan las pruebas y test_player) ===== */

/* Limpia el control y publica magic/version con hps_state = BOOTING. */
void shm_hps_init(volatile shm_t *s);

/* Publica la tabla (ya escrita en s->tracks) con track_count pistas y estado READY. */
void shm_hps_publish(volatile shm_t *s, uint32_t track_count);

/* 1 si hay un comando nuevo; lo devuelve en cmd/arg/seq. */
int shm_hps_poll(volatile const shm_t *s, uint32_t *cmd, uint32_t *arg, uint32_t *seq);

/* Atiende el comando seq: descarta el ring y arranca un flujo en el estado dado. */
void shm_hps_ack(volatile shm_t *s, uint32_t seq, uint32_t stream_state);

/* Espacio libre en el ring (cuadros). */
uint32_t shm_hps_space(volatile const shm_t *s);

/* Escribe hasta n cuadros. Devuelve los escritos. */
uint32_t shm_hps_write(volatile shm_t *s, const uint32_t *frames, uint32_t n);

/* Marca el fin del flujo actual (despues del ultimo shm_hps_write). */
void shm_hps_end(volatile shm_t *s, uint32_t stream_state);

#endif /* COMMON_SHM_PROTO_H */
