/*
 * Driver del IP Audio and Video Config (altera_up_avalon_audio_and_video_config)
 * para el codec WM8731 de la DE1-SoC (NV-07). Sin HAL.
 *
 * Mapa de registros ("Audio/Video Configuration Core", tabla 1):
 *   0x0 control  bit0 R (reinicia y repite la autoinicializacion),
 *                [23:16] DEV (0 = codec de audio)
 *   0x4 status   bit0 ACK (1 = error en la transferencia I2C), bit1 RDY (listo
 *                para otra transferencia), bit8 AIS (autoinicializacion completa)
 *   0x8 address  [6:0] registro del WM8731
 *   0xC data     [8:0] dato de 9 bits del registro
 *
 * El IP ya deja el codec configurado al arrancar (CD-07): maestro, Left Justified,
 * 16 bits, 48 kHz con MCLK = 12.288 MHz. codec_init() espera esa configuracion y
 * corrige lo que el IP no permite elegir (de-enfasis, ver codec.c).
 */
#ifndef DRIVERS_CODEC_H
#define DRIVERS_CODEC_H

#include <stdint.h>

/* Registros del WM8731 (datasheet, "Register Map"). */
#define WM8731_R2_LHP_OUT    0x02
#define WM8731_R3_RHP_OUT    0x03
#define WM8731_R4_ANALOG     0x04
#define WM8731_R5_DIGITAL    0x05
#define WM8731_R6_POWER      0x06
#define WM8731_R7_FORMAT     0x07
#define WM8731_R8_SAMPLING   0x08
#define WM8731_R9_ACTIVE     0x09

#define WM8731_R5_DACMU      (1u << 3)   /* silencio digital del DAC */

#define CODEC_OK          0
#define CODEC_ERR_TIMEOUT (-1)
#define CODEC_ERR_NACK    (-2)

/* Espera a que termine la autoinicializacion (AIS) y quita la de-enfasis.
 * timeout: iteraciones de sondeo (cada una lee el registro de estado). */
int codec_init(uintptr_t base, uint32_t timeout);

/* Escribe el registro reg (7 bits) del codec con value (9 bits) por I2C. */
int codec_write(uintptr_t base, uint32_t reg, uint32_t value, uint32_t timeout);

/* Silencia o habilita la salida del DAC (bit DACMU de R5). */
int codec_mute(uintptr_t base, int mute, uint32_t timeout);

#endif /* DRIVERS_CODEC_H */
