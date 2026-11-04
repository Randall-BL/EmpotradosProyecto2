#include "evq.h"

/* Barrera del compilador: el dato del slot debe quedar escrito antes de publicar el
 * indice (y leido antes de liberarlo). En un solo nucleo basta con que el compilador
 * no reordene; los indices y el buffer son volatile. */
#define EVQ_BARRIER() __asm__ __volatile__("" ::: "memory")

void evq_init(evq_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->dropped = 0;
}

err_t evq_push(evq_t *q, uint8_t ev)
{
    uint32_t h = q->head;
    if (h - q->tail >= EVQ_SIZE) {
        q->dropped = q->dropped + 1u;
        return ERR_FULL;
    }
    q->buf[h & (EVQ_SIZE - 1u)] = ev;
    EVQ_BARRIER();
    q->head = h + 1u;
    return ERR_OK;
}

err_t evq_pop(evq_t *q, uint8_t *ev)
{
    uint32_t t = q->tail;
    if (q->head == t)
        return ERR_EMPTY;
    *ev = q->buf[t & (EVQ_SIZE - 1u)];
    EVQ_BARRIER();
    q->tail = t + 1u;
    return ERR_OK;
}

uint32_t evq_count(const evq_t *q)
{
    return q->head - q->tail;
}
