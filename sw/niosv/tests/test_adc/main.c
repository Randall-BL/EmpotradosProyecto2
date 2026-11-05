/*
 * Prueba del driver del ADC (HW-08, NV-08, PR-05) para sim/questa/tb_adc.sv.
 *
 * Lee continuamente el canal del potenciometro (CH0) y lo muestra en LEDR con
 * 10 bits (valor >> 2). El testbench modela el AD7928 o el LTC2308 (las dos
 * revisiones de la DE1-SoC), mueve el potenciometro y espera cada valor en LEDR
 * con un limite de tiempo.
 */
#include <stdint.h>
#include "system.h"
#include "adc.h"
#include "pio.h"

#define POT_CHANNEL 0u

int main(void)
{
    adc_init(ADC_BASE);
    for (;;)
        pio_write(PIO_LED_BASE, adc_read(ADC_BASE, POT_CHANNEL) >> 2);
}
