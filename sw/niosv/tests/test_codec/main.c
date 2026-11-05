/*
 * Prueba del driver del Audio and Video Config (NV-07, PR-05) para
 * sim/questa/tb_codec.sv.
 *
 * codec_init() espera la autoinicializacion del IP y quita la de-enfasis
 * (R5 = 0x000); luego se prueba codec_mute(). El testbench modela el codec como
 * esclavo I2C (direccion 0x34) y verifica cada registro recibido.
 *
 * LEDR al final: 0x0C0 si todo devolvio CODEC_OK, 0x0E0 | codigo si fallo.
 */
#include <stdint.h>
#include "system.h"
#include "codec.h"
#include "pio.h"

/* Iteraciones de sondeo: la autoinicializacion son 10 escrituras I2C a ~24 kHz
 * (~12 ms); cada lectura del estado tarda unos pocos ciclos a 50 MHz. */
#define TIMEOUT 5000000u

int main(void)
{
    int rc;

    pio_write(PIO_LED_BASE, 0x0C1);
    rc = codec_init(AV_CONFIG_BASE, TIMEOUT);
    if (rc == CODEC_OK)
        rc = codec_mute(AV_CONFIG_BASE, 1, TIMEOUT);
    if (rc == CODEC_OK)
        rc = codec_mute(AV_CONFIG_BASE, 0, TIMEOUT);

    pio_write(PIO_LED_BASE, rc == CODEC_OK ? 0x0C0u : (0x0E0u | ((uint32_t)-rc & 0xFu)));
    for (;;) {
    }
}
