/*
 * Acceso a registros mapeados a memoria por puntero directo.
 *
 * Restriccion del enunciado (§3-SW p.5): nada de HAL (alt_*) ni de las macros
 * IOWR/IORD. Todos los drivers de sw/niosv/drivers/ leen y escriben los
 * registros solo con HW_REG(); las bases salen de system.h del BSP.
 */
#ifndef DRIVERS_HW_H
#define DRIVERS_HW_H

#include <stdint.h>

/* Registro de 32 bits en base + desplazamiento en bytes. volatile para que el
 * compilador no elimine ni reordene los accesos al periferico. */
#define HW_REG(base, off) (*(volatile uint32_t *)((uintptr_t)(base) + (uintptr_t)(off)))

/* Registro de 8 bits (p. ej. un caracter del Character Buffer). */
#define HW_REG8(base, off) (*(volatile uint8_t *)((uintptr_t)(base) + (uintptr_t)(off)))

#endif /* DRIVERS_HW_H */
