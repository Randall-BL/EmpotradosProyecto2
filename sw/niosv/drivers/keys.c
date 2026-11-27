/* Botones KEY[3:0] por interrupcion (NV-03). Ver keys.h. */
#include "keys.h"
#include "irq.h"
#include "pio.h"

/* Estado del driver: lo recibe la ISR como contexto. */
typedef struct {
    uintptr_t base;            /* base del PIO de los botones */
    evq_t *queue;              /* cola donde se encolan los eventos */
    volatile uint32_t irqs;    /* interrupciones atendidas */
} keys_t;

static keys_t keys;

/* ISR: solo limpia la fuente y encola. Un flanco que llegue durante la ISR queda en
 * edgecapture (pio_edges_take limpia solo lo que leyo) y vuelve a disparar la IRQ. */
static void keys_isr(void *ctx)
{
    keys_t *k = (keys_t *)ctx;
    uint32_t edges = pio_edges_take(k->base) & KEYS_MASK;
    k->irqs = k->irqs + 1u;
    /* Un evento por cada boton con flanco: el codigo del evento es el numero de KEY. */
    for (uint32_t i = 0; i < 4u; ++i)
        if (edges & (1u << i))
            (void)evq_push(k->queue, (uint8_t)i);
}

/*
 * Configura los botones: apaga sus IRQ, descarta flancos viejos, registra la ISR y
 * por ultimo habilita la IRQ de los 4 botones en el PIO. Devuelve el resultado de
 * irq_attach() (0 = bien).
 */
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

/* Cantidad de interrupciones de botones atendidas desde keys_init(). */
uint32_t keys_irq_count(void)
{
    return keys.irqs;
}
