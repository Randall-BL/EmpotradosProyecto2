/*
 * Modelo del HPS para probar sin tarjeta (pruebas del PC y simulacion del Nios V).
 *
 * Hace del lado productor del protocolo (shm_proto.h) con pistas sinteticas: publica
 * una tabla de n pistas ("Cancion 01".."Cancion NN") y, al recibir PLAY, entrega los
 * cuadros de la pista. Cada cuadro lleva su identidad para verificar el flujo:
 *   izquierdo = posicion dentro de la pista (16 bits bajos)
 *   derecho   = 0x4000 | indice de la pista
 * Sirve tambien de referencia para el productor real (HP-07, persona D).
 */
#ifndef COMMON_HPS_MODEL_H
#define COMMON_HPS_MODEL_H

#include <stdint.h>
#include "shm_proto.h"

#define HPS_MODEL_TAG 0x4000u

typedef struct {
    volatile shm_t *s;
    uint32_t ntracks;
    uint32_t frames;                       /* cuadros por pista */
    uint32_t bad_mask;                     /* bit i = la pista i "no se puede leer" */
    uint32_t streaming;                    /* 1 mientras entrega una pista */
    uint32_t track, pos;
    uint32_t commands;                     /* comandos atendidos */
} hps_model_t;

/* Inicializa shm y publica la tabla (duration_s = frames / 48000). */
void hps_model_init(hps_model_t *m, volatile shm_t *s, uint32_t ntracks, uint32_t frames,
                    uint32_t bad_mask);

/* Atiende un comando pendiente y escribe hasta max_frames cuadros. */
void hps_model_poll(hps_model_t *m, uint32_t max_frames);

/* Cuadro que el modelo entrega en la posicion pos de la pista track. */
static inline uint32_t hps_model_frame(uint32_t track, uint32_t pos)
{
    return (pos & 0xFFFFu) | ((HPS_MODEL_TAG | track) << 16);
}

#endif /* COMMON_HPS_MODEL_H */
