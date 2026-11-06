/*
 * Driver de los botones KEY[3:0] por interrupcion (NV-03).
 *
 * PIO pio_key (UG-01085, "PIO Core"): flanco de bajada con edgecapture que se limpia
 * escribiendo 1 (bitClearingEdgeCapReg = 1 en build_system.tcl). La ISR es corta:
 * lee y limpia la fuente y encola un evento APP_EV_KEY(n) por cada boton (SC-09); el
 * antirrebote (NV-04) y la logica quedan en el lazo principal.
 *
 * Unico uso del HAL: irq_attach() (alt_ic_isr_register, permitido por §3-SW p.5).
 */
#ifndef DRIVERS_KEYS_H
#define DRIVERS_KEYS_H

#include <stdint.h>
#include "evq.h"

#define KEYS_MASK 0xFu

/* Limpia flancos viejos, registra la ISR y habilita las IRQ de los 4 botones.
 * Devuelve 0 si se pudo registrar la ISR. */
int keys_init(uintptr_t base, uint32_t ic_id, uint32_t irq, evq_t *queue);

/* Interrupciones atendidas (para mediciones y depuracion). */
uint32_t keys_irq_count(void);

#endif /* DRIVERS_KEYS_H */
