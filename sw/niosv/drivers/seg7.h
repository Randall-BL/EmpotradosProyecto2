/*
 * Driver de seg7_time (NV-09): tiempo MM:SS en HEX3..HEX0 y cancion en HEX5..HEX4.
 * El hardware convierte los segundos a MM:SS (docs/hw/verificacion-filtro.md). Sin HAL.
 */
#ifndef DRIVERS_SEG7_H
#define DRIVERS_SEG7_H

#include <stdint.h>

void     seg7_init(uintptr_t base);                       /* encendido, 00:00, sin cancion */
void     seg7_set_time(uintptr_t base, uint32_t seconds); /* >= 6000 se muestra 99:59 */
void     seg7_set_track(uintptr_t base, uint32_t track, int show);
void     seg7_enable(uintptr_t base, int on);
uint32_t seg7_shown_bcd(uintptr_t base);                  /* {M1,M0,S1,S0} mostrado */

#endif /* DRIVERS_SEG7_H */
