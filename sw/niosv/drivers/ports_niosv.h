/*
 * Puertos de la aplicacion (SC-14) implementados con los drivers del Nios V.
 *
 * Cada funcion es un adaptador de una o dos lineas sobre un driver sin HAL; asi la
 * logica de sw/common/src/app.c corre igual aqui y en las pruebas del PC.
 *   audio:  filtro_write -> filtro_read -> audio_write_frame (CD-04: el filtro va
 *           intercalado con la FIFO del codec, sin esperar)
 *   fx:     filtro_config (banco / bypass) y filtro_set_gain (volumen, CD-05)
 *   panel:  seg7 (MM:SS y numero de pista) y LEDR
 *   input:  SW y canal 0 del ADC (potenciometro)
 *   screen: Character Buffer (vga_text)
 *   clock:  timebase (mtime)
 */
#ifndef DRIVERS_PORTS_NIOSV_H
#define DRIVERS_PORTS_NIOSV_H

#include "ports.h"
#include "vga_text.h"

/* Llena p con los puertos del hardware (la fuente se agrega aparte: shm_source).
 * vga debe vivir mientras se use la pantalla. */
void ports_niosv_init(app_ports_t *p, vga_text_t *vga);

#endif /* DRIVERS_PORTS_NIOSV_H */
