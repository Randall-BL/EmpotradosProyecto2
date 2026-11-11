/*
 * Fuente de audio sobre la memoria compartida (NV-13, lado Nios V del canal).
 *
 * Implementa source_port_t (ports.h) con el protocolo de shm_proto.h: lee la tabla de
 * pistas, envia los comandos PLAY/STOP y consume el ring. Es portable (recibe el
 * puntero a shm): el Nios V le pasa SHM_BASE y las pruebas del PC una estructura en
 * RAM con el modelo del HPS (hps_model.h) del otro lado.
 */
#ifndef COMMON_SHM_SOURCE_H
#define COMMON_SHM_SOURCE_H

#include <stdint.h>
#include "ports.h"
#include "shm_proto.h"

typedef struct {
    volatile shm_t *s;
    uint32_t seq;                          /* ultimo comando enviado */
    int active;                            /* hay una pista pedida (PLAY sin STOP) */
    /* Copia de los textos de la pista pedida: terminados en '\0' aunque el HPS no lo
     * haga, y estables mientras se dibujan. */
    char title[SHM_TEXT], artist[SHM_TEXT], album[SHM_TEXT];
} shm_source_t;

void shm_source_init(shm_source_t *src, volatile shm_t *s);

/* Llena port con las funciones de esta fuente (ctx = src). */
void shm_source_port(shm_source_t *src, source_port_t *port);

#endif /* COMMON_SHM_SOURCE_H */
