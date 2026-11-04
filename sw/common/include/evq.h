/*
 * Cola de eventos de un productor y un consumidor sin bloqueo (SC-09).
 *
 * El productor es una ISR (botones) y el consumidor el lazo principal. Cada indice lo
 * escribe un solo lado: la ISR escribe head y el lazo escribe tail, asi que no hace
 * falta deshabilitar interrupciones ni exclusion mutua. Los indices corren libres
 * (uint32_t) y se enmascaran con EVQ_SIZE - 1, que debe ser potencia de 2.
 *
 * Si la cola esta llena el evento se descarta y se cuenta en dropped: en una ISR no se
 * puede esperar.
 */
#ifndef COMMON_EVQ_H
#define COMMON_EVQ_H

#include <stdint.h>
#include "err.h"

#define EVQ_SIZE 16u                       /* potencia de 2 */

typedef struct {
    volatile uint8_t buf[EVQ_SIZE];
    volatile uint32_t head;                /* lo escribe solo el productor */
    volatile uint32_t tail;                /* lo escribe solo el consumidor */
    volatile uint32_t dropped;             /* eventos perdidos por cola llena */
} evq_t;

void evq_init(evq_t *q);

/* Productor (ISR). ERR_FULL si no cabe (se cuenta en dropped). */
err_t evq_push(evq_t *q, uint8_t ev);

/* Consumidor. ERR_EMPTY si no hay eventos. */
err_t evq_pop(evq_t *q, uint8_t *ev);

uint32_t evq_count(const evq_t *q);

#endif /* COMMON_EVQ_H */
