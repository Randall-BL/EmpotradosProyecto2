/*
 * Antirrebote por ventana de tiempo (NV-04). Ver debounce.h.
 *
 * Por cada boton se guarda el instante del ultimo flanco aceptado. Un flanco nuevo se
 * acepta solo si paso al menos DEB_WINDOW_MS desde ese instante; si no, se considera
 * rebote y se descarta.
 */
#include "debounce.h"

/* Olvida todos los flancos anteriores: el primer flanco de cada boton se aceptara. */
void deb_init(deb_t *d)
{
    for (uint32_t i = 0; i < DEB_KEYS; ++i) {
        d->last_ms[i] = 0;
        d->seen[i] = 0;
    }
    d->rejected = 0;
}

/*
 * Decide si el flanco del boton key, ocurrido en now_ms, es una pulsacion real.
 * Devuelve 1 si se acepta (y lo registra como el ultimo flanco de ese boton) o 0 si
 * es un rebote o el numero de boton no existe. Los rechazos se cuentan en rejected.
 */
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
