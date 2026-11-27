/*
 * Cola de eventos de un productor (ISR) y un consumidor (lazo principal) sin bloqueo
 * (SC-09). Ver evq.h.
 *
 * head y tail son contadores que solo crecen: la cantidad de eventos en cola es
 * head - tail (la resta modular funciona aunque den la vuelta) y la posicion real en
 * el buffer es el contador enmascarado con EVQ_SIZE - 1.
 */
#include "evq.h"

/* Barrera del compilador: el dato del slot debe quedar escrito antes de publicar el
 * indice (y leido antes de liberarlo). En un solo nucleo basta con que el compilador
 * no reordene; los indices y el buffer son volatile. */
#define EVQ_BARRIER() __asm__ __volatile__("" ::: "memory")

/* Deja la cola vacia y el contador de perdidos en cero. */
void evq_init(evq_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->dropped = 0;
}

/*
 * Lado productor (se llama desde la ISR). Guarda ev al final de la cola. Si esta
 * llena no espera: cuenta el evento como perdido y devuelve ERR_FULL.
 */
err_t evq_push(evq_t *q, uint8_t ev)
{
    uint32_t h = q->head;
    if (h - q->tail >= EVQ_SIZE) {
        q->dropped = q->dropped + 1u;
        return ERR_FULL;
    }
    q->buf[h & (EVQ_SIZE - 1u)] = ev;
    EVQ_BARRIER();
    q->head = h + 1u;                      /* publica el evento al consumidor */
    return ERR_OK;
}

/*
 * Lado consumidor (lazo principal). Saca el evento mas viejo y lo deja en *ev.
 * Devuelve ERR_EMPTY si no hay eventos.
 */
err_t evq_pop(evq_t *q, uint8_t *ev)
{
    uint32_t t = q->tail;
    if (q->head == t)
        return ERR_EMPTY;
    *ev = q->buf[t & (EVQ_SIZE - 1u)];
    EVQ_BARRIER();
    q->tail = t + 1u;                      /* libera el slot para el productor */
    return ERR_OK;
}

/* Cantidad de eventos que esperan en la cola. */
uint32_t evq_count(const evq_t *q)
{
    return q->head - q->tail;
}
