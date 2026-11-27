/* Driver del DE1-SoC ADC Controller (NV-08). Ver adc.h para el mapa de registros. */
#include "adc.h"
#include "hw.h"

/* Desplazamientos de los registros de escritura del IP. */
#define ADC_REG_UPDATE      0x0   /* escribir: una conversion de todos los canales */
#define ADC_REG_AUTO_UPDATE 0x4   /* escribir 1: conversion continua */

/* Activa la conversion continua: desde aqui cada canal siempre tiene un valor reciente
 * y adc_read() nunca tiene que esperar. */
void adc_init(uintptr_t base)
{
    HW_REG(base, ADC_REG_AUTO_UPDATE) = 1;
}

/* Lee el ultimo resultado del canal ch (0..7). Cada canal ocupa un registro de 32 bits
 * en base + 4*ch; el resultado son los 12 bits bajos (0..4095). */
uint32_t adc_read(uintptr_t base, uint32_t ch)
{
    return HW_REG(base, (ch & 7u) * 4u) & 0xFFFu;
}
