/*
 * Unica excepcion permitida al "sin HAL" (§3-SW p.5): registro y habilitacion de
 * interrupciones. Dentro de las ISR los registros se acceden por puntero (pio.h).
 */
#include "irq.h"
#include "sys/alt_irq.h"

int irq_attach(uint32_t ic_id, uint32_t irq, irq_handler_t handler, void *ctx)
{
    /* alt_ic_isr_register tambien habilita la IRQ en el controlador. */
    return alt_ic_isr_register(ic_id, irq, handler, ctx, 0);
}

void irq_enable(uint32_t ic_id, uint32_t irq)
{
    alt_ic_irq_enable(ic_id, irq);
}

void irq_disable(uint32_t ic_id, uint32_t irq)
{
    alt_ic_irq_disable(ic_id, irq);
}
