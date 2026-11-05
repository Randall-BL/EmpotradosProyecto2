/*
 * Registro de rutinas de interrupcion.
 *
 * Es el UNICO punto del software que usa el HAL, y solo con las funciones que el
 * enunciado permite para interrupciones (alt_ic_isr_register, alt_ic_irq_enable,
 * alt_ic_irq_disable). La auditoria NV-17 debe encontrar alt_* solo en irq.c.
 */
#ifndef DRIVERS_IRQ_H
#define DRIVERS_IRQ_H

#include <stdint.h>

typedef void (*irq_handler_t)(void *ctx);

/* Registra handler para la IRQ irq del controlador ic_id (PIO_KEY_IRQ y
 * PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID de system.h) y la habilita.
 * Devuelve 0 si se pudo registrar. */
int irq_attach(uint32_t ic_id, uint32_t irq, irq_handler_t handler, void *ctx);

void irq_enable(uint32_t ic_id, uint32_t irq);
void irq_disable(uint32_t ic_id, uint32_t irq);

#endif /* DRIVERS_IRQ_H */
