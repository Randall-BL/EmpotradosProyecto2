/*
 * Control de volumen por software (SC-11): lectura del potenciometro (ADC de 12
 * bits) -> escalon de volumen con histeresis -> ganancia Q15 -> escalado de las
 * muestras con saturacion.
 *
 * Portable (sw/common/): no incluye system.h ni toca registros; se prueba en el
 * PC con tests/test_volume.c. Si CD-05 mueve el escalado al hardware, este modulo
 * sigue dando la ganancia que se escribe en el registro del modulo propio.
 */
#ifndef COMMON_VOLUME_H
#define COMMON_VOLUME_H

#include <stdint.h>

#define VOLUME_STEPS     32u     /* escalones de 2 dB; 0 = silencio, 31 = 0 dB */
#define VOLUME_ADC_MAX   4095u
#define VOLUME_HYSTERESIS 24u    /* cuentas del ADC: evita que el escalon tiemble */

typedef struct {
    uint32_t step;               /* escalon actual, 0..VOLUME_STEPS-1 */
    uint32_t gain_q15;           /* ganancia del escalon (32768 = 1.0) */
} volume_t;

/* Arranca en el escalon que corresponde a adc, sin histeresis. */
void volume_init(volume_t *v, uint32_t adc);

/* Actualiza con una lectura nueva del ADC. Devuelve 1 si cambio el escalon. */
int volume_update(volume_t *v, uint32_t adc);

/* Ganancia Q15 de un escalon (tabla logaritmica). */
uint32_t volume_gain_of_step(uint32_t step);

/* Porcentaje 0..100 del escalon, para la pantalla. */
uint32_t volume_percent(const volume_t *v);

/* (s * gain_q15) >> 15 con redondeo y saturacion a 16 bits. */
int16_t volume_scale(int16_t s, uint32_t gain_q15);

/* Escala n muestras en sitio. */
void volume_apply(int16_t *samples, uint32_t n, uint32_t gain_q15);

#endif /* COMMON_VOLUME_H */
