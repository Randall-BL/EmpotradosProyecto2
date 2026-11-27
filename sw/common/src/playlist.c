/* Lista de reproduccion: pista actual con vuelta al inicio y al final (SC-07). */
#include "playlist.h"

/* Crea una lista de count pistas con la primera seleccionada. ERR_EMPTY si count = 0,
 * ERR_ARG si p es nulo. */
err_t playlist_init(playlist_t *p, uint32_t count)
{
    if (!p)
        return ERR_ARG;
    p->count = count;
    p->current = 0;
    return count ? ERR_OK : ERR_EMPTY;
}

/* Pasa a la siguiente pista; despues de la ultima vuelve a la primera. */
uint32_t playlist_next(playlist_t *p)
{
    if (p->count)
        p->current = (p->current + 1u >= p->count) ? 0u : p->current + 1u;
    return p->current;
}

/* Pasa a la pista anterior; antes de la primera va a la ultima. */
uint32_t playlist_prev(playlist_t *p)
{
    if (p->count)
        p->current = (p->current == 0u) ? p->count - 1u : p->current - 1u;
    return p->current;
}

/* Selecciona directamente la pista index (0..count-1). */
err_t playlist_select(playlist_t *p, uint32_t index)
{
    if (index >= p->count)
        return ERR_BOUNDS;
    p->current = index;
    return ERR_OK;
}

/*
 * Avance automatico al terminar una pista. Devuelve 1 si se dio la vuelta (el indice
 * nuevo no es mayor que el anterior: de la ultima a la primera, o lista de 1 pista).
 */
int playlist_auto_advance(playlist_t *p)
{
    if (!p->count)
        return 0;
    uint32_t before = p->current;
    return playlist_next(p) <= before;
}

/* Numero de pista para mostrar (empieza en 1); 0 si la lista esta vacia. */
uint32_t playlist_number(const playlist_t *p)
{
    return p->count ? p->current + 1u : 0u;
}

/* 1 si hay al menos PLAYLIST_MIN_TRACKS (10) pistas, como pide el enunciado. */
int playlist_meets_minimum(const playlist_t *p)
{
    return p->count >= PLAYLIST_MIN_TRACKS;
}
