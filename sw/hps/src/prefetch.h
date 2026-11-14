/*
 * Lectura anticipada de la SD (HP-07): un hilo lee el PCM de la pista en curso hacia un
 * buffer circular grande en la DDR3 del HPS, para que el hilo que llena el ring de shm
 * nunca espere a la tarjeta. El ring de shm solo cubre ~170 ms (shm_proto.h) y una
 * lectura de la SD bajo Linux puede tardar mas (cortes de ~1 s medidos el 13 nov).
 *
 * Un solo productor (el hilo lector) y un solo consumidor (producer.c), protegidos por
 * un mutex. Solo usa C estandar + POSIX (pthread): corre igual en el HPS y en el PC.
 */
#ifndef HPS_PREFETCH_H
#define HPS_PREFETCH_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PREFETCH_BYTES   (4u << 20)       /* 4 MiB: ~24 s a 44.1 kHz estereo 16 bits */
#define PREFETCH_CHUNK   (64u << 10)      /* bytes por fread */
#define PREFETCH_PRIME   PREFETCH_CHUNK   /* lo que se espera antes de dar el ack */
#define PREFETCH_SLOW_MS 50u              /* fread mas lento que esto se reporta */

typedef struct {
    pthread_t th;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int ready;                            /* mu y cv inicializados */
    int running;                          /* hay un hilo que unir */
    uint8_t *buf;                         /* PREFETCH_BYTES, se reserva una vez */
    FILE *f;                              /* lo usa solo el hilo mientras corre */
    size_t left;                          /* bytes del archivo que faltan leer */
    size_t head, tail;                    /* bytes escritos / consumidos (libres) */
    int done;                             /* el hilo termino (EOF, error o cancelado) */
    int cancel;
    int verbose;                          /* imprimir las lecturas lentas */
    uint32_t track;                       /* solo para los mensajes */

    /* estadisticas de la pista en curso */
    uint32_t reads, slow_reads, max_read_ms;
    size_t min_level;                     /* menor ocupacion vista al consumir */
} prefetch_t;

/* Arranca la lectura de bytes bytes desde la posicion actual de f (que pasa a ser del
 * prefetch y se cierra en prefetch_stop). 0 si arranco. */
int prefetch_start(prefetch_t *p, FILE *f, size_t bytes, uint32_t track);

/* Bloquea hasta tener al menos min bytes o hasta que el hilo termine. */
void prefetch_wait(prefetch_t *p, size_t min);

/* Copia hasta max bytes disponibles, en multiplos de align. No bloquea. */
size_t prefetch_read(prefetch_t *p, void *dst, size_t max, size_t align);

/* 1 si el hilo termino y quedan menos de align bytes: fin de la pista. */
int prefetch_finished(prefetch_t *p, size_t align);

/* Bytes disponibles ahora. */
size_t prefetch_level(prefetch_t *p);

/* Cancela el hilo, lo une y cierra el archivo. Se puede llamar sin flujo activo. */
void prefetch_stop(prefetch_t *p);

#endif /* HPS_PREFETCH_H */
