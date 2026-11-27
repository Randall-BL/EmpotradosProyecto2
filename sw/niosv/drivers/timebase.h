/*
 * Base de tiempo del Nios V (NV-12): milisegundos y ciclos a partir de mtime.
 *
 * mtime es el contador del timer_sw_agent del Nios V (ALT_CPU_MTIME_OFFSET en
 * system.h); cuenta a la frecuencia del CPU (50 MHz). Se lee por puntero, sin HAL.
 * El Nios V/m no implementa el CSR mcycle (leerlo da una excepcion, ver test_ciclos):
 * para medir ciclos se usa la palabra baja de mtime, que avanza 1 por ciclo.
 * La conversion a ms no divide (clock.h, sw/common).
 */
#ifndef DRIVERS_TIMEBASE_H
#define DRIVERS_TIMEBASE_H

#include <stdint.h>

/* ticks_per_ms: 0 = ALT_CPU_CPU_FREQ / 1000 (tiempo real). Un valor menor acelera el
 * reloj; solo lo usan los programas de prueba en simulacion. */
void timebase_init(uint32_t ticks_per_ms);

/* Milisegundos desde timebase_init. Llamar al menos cada 85 s. */
uint32_t timebase_ms(void);

/* Ciclos de CPU (palabra baja de mtime): para medir duraciones cortas por resta. */
uint32_t timebase_cycles(void);

#endif /* DRIVERS_TIMEBASE_H */
