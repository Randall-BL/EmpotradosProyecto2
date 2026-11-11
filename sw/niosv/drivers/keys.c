#include "keys.h"
#include "irq.h"
#include "pio.h"

typedef struct {
    uintptr_t base;
    evq_t *queue;
    volatile uint32_t irqs;
} keys_t;

static keys_t keys;

/* ISR: solo limpia la fuente y encola. Un flanco que llegue durante la ISR queda en
 * edgecapture (pio_edges_take limpia solo lo que leyo) y vuelve a disparar la IRQ. */
static void keys_isr(void *ctx)
{
    keys_t *k = (keys_t *)ctx;
    uint32_t edges = pio_edges_take(k->base) & KEYS_MASK;
    k->irqs = k->irqs + 1u;
    for (uint32_t i = 0; i < 4u; ++i)
        if (edges & (1u << i))
            (void)evq_push(k->queue, (uint8_t)i);
}

int keys_init(uintptr_t base, uint32_t ic_id, uint32_t irq, evq_t *queue)
{
    keys.base = base;
    keys.queue = queue;
    keys.irqs = 0;
    pio_irq_mask(base, 0);
    (void)pio_edges_take(base);                  /* flancos de antes del arranque */
    int r = irq_attach(ic_id, irq, keys_isr, &keys);
    pio_irq_mask(base, KEYS_MASK);
    return r;
}

uint32_t keys_irq_count(void)
{
    return keys.irqs;
}
