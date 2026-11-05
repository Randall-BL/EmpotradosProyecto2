/*
 * Prueba de humo del Nios V/m sobre el SoC base del proyecto.
 *
 * Origen: Taller 9, sw/src/hello_world.c (ver docs/reutilizado-taller9.md).
 * Imprime por la JTAG UART (STDOUT del BSP) y verifica que el procesador
 * arranca y ejecuta codigo C en simulacion RTL (Questa), sin tarjeta fisica.
 * Luego escribe y lee los PIO con punteros directos a sus direcciones (ver
 * system.h del BSP generado), SIN usar las funciones del driver HAL (alt_*)
 * ni las macros IOWR/IORD.
 *
 * Se reemplaza por el lazo principal del reproductor (TODO.md, NV-*); hasta
 * entonces sirve para comprobar que el flujo build-soc/build-niosv/sim-soc
 * funciona con la estructura del proyecto.
 */

#include <stdio.h>
#include <stdint.h>
#include "system.h"   /* generado por el BSP: define las bases (XXX_BASE) */

int main(void)
{
    for (int i = 0; i < 5; ++i) {
        printf("Hola mundo, aqui el Nios V/m reportandose %d...\n", i);
    }

    /*
     * Acceso a los PIO por puntero directo (Christopher Rodriguez, 2022040771).
     *
     * Las bases salen de system.h del BSP:
     *   PIO_OUT_BASE = 0x00040010  (8 bits, salida -> 8 LED)
     *   PIO_IN_BASE  = 0x00040020  (4 bits, entrada <- 4 interruptores)
     * El registro de datos de cada PIO esta en el desplazamiento 0. Los
     * punteros son volatile para que el compilador no elimine ni reordene
     * los accesos. No se usan IOWR/IORD ni funciones alt_*.
     */
    volatile uint32_t *pio_out = (volatile uint32_t *)PIO_OUT_BASE;
    volatile uint32_t *pio_in  = (volatile uint32_t *)PIO_IN_BASE;

    const uint32_t patrones[] = { 0x01, 0xA5, 0xFF };
    for (unsigned i = 0; i < sizeof patrones / sizeof patrones[0]; ++i) {
        *pio_out = patrones[i];                /* escribe el registro de datos */
        printf("PIO_OUT <= 0x%02lx\n", (unsigned long)patrones[i]);
    }

    uint32_t v = *pio_in & 0xF;                /* lee el registro de datos (4 bits) */
    printf("PIO_IN  = 0x%08lx\n", (unsigned long)v);

    printf("Fin del programa.\n");
    return 0;
}
