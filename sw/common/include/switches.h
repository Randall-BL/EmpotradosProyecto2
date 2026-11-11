/*
 * Switches -> filtro activo (CD-08, NV-05).
 *
 * Un switch por filtro (lectura periodica cada 20 ms, ver docs/decisiones/CD-08.md):
 *
 *   SW2 SW1 SW0   filtro
 *    0   0   0    bypass (sin filtro)
 *    0   0   1    pasa-bajos 1 kHz      (modulo propio, banco 0)
 *    0   1   0    pasa-banda 1-4 kHz    (modulo propio, banco 1)
 *    1   0   0    reverberacion 50 ms   (software, Nios V)
 *    dos o mas    combinacion invalida: bypass y LEDR9 encendido
 *
 * SW[9:3] no se usan. El filtro solo cambia cuando la lectura se repite igual dos
 * veces seguidas (20 ms): un switch que rebota no hace saltar entre filtros.
 */
#ifndef COMMON_SWITCHES_H
#define COMMON_SWITCHES_H

#include <stdint.h>

typedef enum {
    FILTER_BYPASS = 0,
    FILTER_LOWPASS,
    FILTER_BANDPASS,
    FILTER_REVERB,
    FILTER_COUNT
} filter_id_t;

#define SW_FILTER_MASK 0x7u               /* SW[2:0] */
#define SW_POLL_MS     20u                /* periodo de lectura (CD-08) */

typedef struct {
    uint32_t last_raw;                    /* ultima lectura de SW[2:0] */
    uint32_t stable_raw;                  /* lectura aceptada */
    filter_id_t filter;
    int invalid;                          /* 1 si stable_raw tiene mas de un bit */
} sw_state_t;

/* Traduce SW[2:0] a filtro. *invalid = 1 si hay mas de un switch arriba. */
filter_id_t sw_decode(uint32_t sw, int *invalid);

void sw_init(sw_state_t *s, uint32_t sw);

/* Lectura periodica. Devuelve 1 si cambio el filtro o la marca de invalido. */
int sw_update(sw_state_t *s, uint32_t sw);

/* Nombre para la pantalla (<= 24 caracteres). */
const char *sw_filter_name(filter_id_t f);

#endif /* COMMON_SWITCHES_H */
