/*
 * Driver del DE1-SoC ADC Controller (altera_up_avalon_de1_soc_adc) para el
 * potenciometro de volumen (NV-08). Sin HAL.
 *
 * Mapa de registros ("DE1-SoC ADC Controller", tabla 1):
 *   0x00 + 4*n  lectura: CH_n, resultado de 12 bits del canal n
 *   0x00        escritura: Update, inicia una conversion de todos los canales
 *   0x04        escritura: Auto-Update, 1 = conversion continua
 *
 * Con Auto-Update activo, una lectura durante una conversion devuelve el valor
 * anterior sin detener el bus, asi que leer el volumen nunca frena al audio.
 */
#ifndef DRIVERS_ADC_H
#define DRIVERS_ADC_H

#include <stdint.h>

#define ADC_MAX 4095u

/* Activa la conversion continua de los canales configurados en el SoC. */
void adc_init(uintptr_t base);

/* Ultimo valor convertido del canal ch (0..7), 12 bits. */
uint32_t adc_read(uintptr_t base, uint32_t ch);

#endif /* DRIVERS_ADC_H */
