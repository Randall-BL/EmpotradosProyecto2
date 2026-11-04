#include "playlist.h"

err_t playlist_init(playlist_t *p, uint32_t count)
{
    if (!p)
        return ERR_ARG;
    p->count = count;
    p->current = 0;
    return count ? ERR_OK : ERR_EMPTY;
}

uint32_t playlist_next(playlist_t *p)
{
    if (p->count)
        p->current = (p->current + 1u >= p->count) ? 0u : p->current + 1u;
    return p->current;
}

uint32_t playlist_prev(playlist_t *p)
{
    if (p->count)
        p->current = (p->current == 0u) ? p->count - 1u : p->current - 1u;
    return p->current;
}

err_t playlist_select(playlist_t *p, uint32_t index)
{
    if (index >= p->count)
        return ERR_BOUNDS;
    p->current = index;
    return ERR_OK;
}

int playlist_auto_advance(playlist_t *p)
{
    if (!p->count)
        return 0;
    uint32_t before = p->current;
    return playlist_next(p) <= before;
}

uint32_t playlist_number(const playlist_t *p)
{
    return p->count ? p->current + 1u : 0u;
}

int playlist_meets_minimum(const playlist_t *p)
{
    return p->count >= PLAYLIST_MIN_TRACKS;
}
