/*
 * Antirrebote por ventana de tiempo (NV-04).
 *
 * Los KEY[3:0] de la DE1-SoC ya traen antirrebote por hardware (disparador de Schmitt,
 * DE1-SoC User Manual, sec. 3.6.1), asi que normalmente llega un solo flanco por
 * pulsacion. Esta ventana es una segunda defensa barata: acepta el primer flanco de
 * cada boton y descarta los que lleguen en los DEB_WINDOW_MS siguientes. No agrega
 * latencia (el primer flanco se atiende de inmediato).
 */
#ifndef COMMON_DEBOUNCE_H
#define COMMON_DEBOUNCE_H

#include <stdint.h>

#define DEB_KEYS      4u
#define DEB_WINDOW_MS 50u

typedef struct {
    uint32_t last_ms[DEB_KEYS];
    uint8_t seen[DEB_KEYS];
    uint32_t rejected;                    /* flancos descartados (para medir) */
} deb_t;

void deb_init(deb_t *d);

/* 1 si el flanco del boton key en now_ms se acepta. */
int deb_accept(deb_t *d, uint32_t key, uint32_t now_ms);

#endif /* COMMON_DEBOUNCE_H */
