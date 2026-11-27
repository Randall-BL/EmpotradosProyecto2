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

#define DEB_KEYS      4u                /* KEY0..KEY3 */
#define DEB_WINDOW_MS 50u               /* flancos a menos de 50 ms del anterior = rebote */

typedef struct {
    uint32_t last_ms[DEB_KEYS];           /* instante del ultimo flanco aceptado de cada boton */
    uint8_t seen[DEB_KEYS];               /* 1 si el boton ya tuvo algun flanco aceptado */
    uint32_t rejected;                    /* flancos descartados (para medir) */
} deb_t;

/* Olvida la historia: el siguiente flanco de cada boton se acepta. */
void deb_init(deb_t *d);

/* 1 si el flanco del boton key en now_ms se acepta. */
int deb_accept(deb_t *d, uint32_t key, uint32_t now_ms);

#endif /* COMMON_DEBOUNCE_H */
