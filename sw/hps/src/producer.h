/*
 * Productor del HPS (HP-06, HP-07, HP-09): recorre la carpeta de canciones de la SD,
 * publica la tabla de pistas en shm y, cuando el Nios V pide una pista, lee su PCM por
 * bloques, lo convierte a 48 kHz estereo (resample.h) y llena el ring (shm_proto.h).
 *
 * El PCM no se lee de la SD en este hilo: lo trae por adelantado prefetch.c (un hilo
 * lector con varios segundos de buffer en la DDR3), asi una lectura lenta de la tarjeta
 * no vacia el ring.
 *
 * Solo usa C estandar + POSIX (stdio, dirent, pthread): corre igual en Linux del HPS y
 * en el PC, donde lo prueba tests/test_producer.c contra un shm en RAM.
 */
#ifndef HPS_PRODUCER_H
#define HPS_PRODUCER_H

#include <stdint.h>
#include <stdio.h>
#include "prefetch.h"
#include "resample.h"
#include "shm_proto.h"

#define PRODUCER_PATH_MAX 256u            /* largo maximo de la ruta de un archivo */
#define PRODUCER_IN_FRAMES 1024u          /* cuadros de entrada por lectura */

/* Lo que hace falta para volver a abrir y leer el PCM de una pista. */
typedef struct {
    char path[PRODUCER_PATH_MAX];         /* ruta completa del .wav */
    uint32_t data_offset;                 /* donde empieza el PCM en el archivo */
    uint32_t frames;                      /* cuadros del archivo */
    uint32_t rate;                        /* frecuencia de muestreo del archivo (Hz) */
    uint16_t channels;                    /* 1 o 2 */
} producer_file_t;

/* Estado completo del productor. */
typedef struct {
    volatile shm_t *s;                    /* memoria compartida (mapeo de /dev/mem) */
    uint32_t ntracks;                     /* pistas validas en la tabla */
    uint32_t hps_state;                   /* SHM_HPS_* que se publica */
    producer_file_t files[SHM_MAX_TRACKS];
    shm_track_t table[SHM_MAX_TRACKS];    /* copia en RAM para volver a publicarla */

    /* flujo en curso */
    int active;                           /* hay una pista abierta */
    prefetch_t pf;                        /* lector anticipado de la SD */
    uint32_t track;                       /* indice de la pista en curso */
    uint32_t in_left;                     /* cuadros del archivo que faltan leer */
    resample_t rs;                        /* conversion a 48 kHz */
    int16_t in[PRODUCER_IN_FRAMES * 2];   /* bloque de PCM leido (intercalado) */
    uint32_t in_pos, in_len;              /* cuadros pendientes en in[] */
    /* Salida del remuestreo: hasta x6 cuadros (8 kHz -> 48 kHz) mas un margen. */
    uint32_t out[PRODUCER_IN_FRAMES * 6 + 8];

    /* diagnostico (HP-07): solo imprime si verbose */
    int verbose;
    int wrote;                            /* ya se entrego algo de la pista en curso */
    int starving;                         /* el ring esta vacio ahora */
    uint32_t ring_empty;                  /* veces que el ring se vacio en la pista */

    uint32_t commands;                    /* comandos atendidos */
    uint32_t skipped;                     /* .wav descartados por formato o lectura */
} producer_t;

/*
 * Recorre dir (sin subcarpetas): toma los .wav en orden alfabetico, hasta
 * SHM_MAX_TRACKS, y descarta los que no se pueden leer o no estan soportados (HP-09).
 * Devuelve las pistas validas. Si dir no existe, hps_state = SHM_HPS_NO_SD.
 */
uint32_t producer_scan(producer_t *p, const char *dir);

/* Inicializa shm (shm_hps_init) y publica la tabla de producer_scan. Corta el flujo. */
void producer_publish(producer_t *p, volatile shm_t *s);

/* Atiende un comando pendiente y escribe en el ring lo que quepa. 1 si hizo algo. */
int producer_poll(producer_t *p);

/* Termina el flujo en curso (hilo lector incluido), si hay. */
void producer_stop(producer_t *p);

#endif /* HPS_PRODUCER_H */
