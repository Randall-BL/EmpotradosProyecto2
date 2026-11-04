/*
 * Lista de reproduccion (SC-07): indice actual, siguiente/anterior con vuelta al
 * inicio, avance automatico al terminar una pista e indice "n/total".
 *
 * No guarda los metadatos (los publica el HPS en la tabla de pistas, SC-13): solo el
 * numero de pistas y la actual. El enunciado pide >= 10 canciones (§3-F p.3); la
 * lista funciona con cualquier cantidad >= 1 y playlist_meets_minimum() dice si se
 * cumple el minimo.
 */
#ifndef COMMON_PLAYLIST_H
#define COMMON_PLAYLIST_H

#include <stdint.h>
#include "err.h"

#define PLAYLIST_MIN_TRACKS 10u

typedef struct {
    uint32_t count;                       /* pistas en la lista */
    uint32_t current;                     /* 0..count-1 */
} playlist_t;

/* ERR_EMPTY si count == 0 (la lista queda vacia y current = 0). */
err_t playlist_init(playlist_t *p, uint32_t count);

/* Avanza o retrocede una pista con vuelta. Devuelve el indice nuevo. */
uint32_t playlist_next(playlist_t *p);
uint32_t playlist_prev(playlist_t *p);

/* Salta a una pista; ERR_BOUNDS si no existe. */
err_t playlist_select(playlist_t *p, uint32_t index);

/* Avance automatico al terminar la pista actual. Da la vuelta al llegar al final (la
 * lista se repite: PR-08 pide dos vueltas). Devuelve 1 si dio la vuelta. */
int playlist_auto_advance(playlist_t *p);

/* Numero para mostrar: 1..count (0 si la lista esta vacia). */
uint32_t playlist_number(const playlist_t *p);

int playlist_meets_minimum(const playlist_t *p);

#endif /* COMMON_PLAYLIST_H */
