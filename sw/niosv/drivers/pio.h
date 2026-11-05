/*
 * Driver del PIO de Altera (altera_avalon_pio) por puntero directo.
 *
 * Mapa de registros (Embedded Peripherals IP User Guide, UG-01085, "PIO Core"):
 *   0x0 data        lectura: entradas · escritura: salidas
 *   0x4 direction   no se usa (los PIO del SoC tienen direccion fija)
 *   0x8 interruptmask  1 = el bit genera IRQ
 *   0xC edgecapture    1 = hubo flanco; se limpia escribiendo 1 en el bit
 *                      (bitClearingEdgeCapReg = 1 en build_system.tcl)
 */
#ifndef DRIVERS_PIO_H
#define DRIVERS_PIO_H

#include <stdint.h>
#include "hw.h"

#define PIO_REG_DATA     0x0
#define PIO_REG_IRQ_MASK 0x8
#define PIO_REG_EDGE_CAP 0xC

static inline uint32_t pio_read(uintptr_t base)
{
    return HW_REG(base, PIO_REG_DATA);
}

static inline void pio_write(uintptr_t base, uint32_t value)
{
    HW_REG(base, PIO_REG_DATA) = value;
}

static inline void pio_irq_mask(uintptr_t base, uint32_t mask)
{
    HW_REG(base, PIO_REG_IRQ_MASK) = mask;
}

/* Devuelve los flancos capturados y los limpia. Se limpia solo lo que se leyo:
 * un flanco que llegue entre la lectura y la escritura no se pierde. */
static inline uint32_t pio_edges_take(uintptr_t base)
{
    uint32_t edges = HW_REG(base, PIO_REG_EDGE_CAP);
    HW_REG(base, PIO_REG_EDGE_CAP) = edges;
    return edges;
}

#endif /* DRIVERS_PIO_H */
