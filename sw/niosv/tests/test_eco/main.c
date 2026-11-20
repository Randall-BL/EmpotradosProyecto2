/*
 * Diagnostico de entradas/salidas en la tarjeta (sin interrupciones ni etapas):
 *   1. Enciende los 10 LEDs (0x3FF) mientras los switches no cambien de como
 *      estaban al arrancar: prueba de cada LED sin depender de las entradas.
 *   2. Al primer cambio de un switch copia continuamente SW a LEDR (LEDR = SW),
 *      para ver cada switch por separado.
 * Sirve para aislar una falla de un switch o de un LED de la logica de test_pio.
 */
#include <stdint.h>
#include "system.h"
#include "pio.h"

int main(void)
{
    uint32_t inicial = pio_read(PIO_SW_BASE) & 0x3FFu;

    pio_write(PIO_LED_BASE, 0x3FFu);
    while ((pio_read(PIO_SW_BASE) & 0x3FFu) == inicial) {
    }
    for (;;) {
        pio_write(PIO_LED_BASE, pio_read(PIO_SW_BASE) & 0x3FFu);
    }
}
