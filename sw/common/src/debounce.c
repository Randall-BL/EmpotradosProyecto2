#include "debounce.h"

void deb_init(deb_t *d)
{
    for (uint32_t i = 0; i < DEB_KEYS; ++i) {
        d->last_ms[i] = 0;
        d->seen[i] = 0;
    }
    d->rejected = 0;
}

int deb_accept(deb_t *d, uint32_t key, uint32_t now_ms)
{
    if (key >= DEB_KEYS)
        return 0;
    /* La resta en uint32_t funciona aunque el contador de ms de la vuelta. */
    if (d->seen[key] && now_ms - d->last_ms[key] < DEB_WINDOW_MS) {
        d->rejected++;
        return 0;
    }
    d->seen[key] = 1;
    d->last_ms[key] = now_ms;
    return 1;
}
