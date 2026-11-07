/*
 * Driver del modulo propio filtro_audio (NV-10, NV-15). Sin HAL.
 * Registros y tiempos: docs/hw/modulo-filtro.md.
 *
 * Uso en el lazo de audio (CD-04, con pipelining para no esperar al filtro):
 *     filtro_write(base, x_i);          // arranca el par i (~136 ciclos)
 *     audio_write_frame(..., y_prev);   // mientras tanto, se entrega el par anterior
 *     y_prev = filtro_read(base);       // ya esta listo (o espera lo que falte)
 * filtro_read() nunca devuelve un resultado a medias: el hardware retiene la lectura
 * con waitrequest mientras procesa.
 */
#ifndef DRIVERS_FILTRO_H
#define DRIVERS_FILTRO_H

#include <stdint.h>

#define FILTRO_BANK_LOWPASS  0u    /* pasa-bajos 1 kHz */
#define FILTRO_BANK_BANDPASS 1u    /* pasa-banda 1-4 kHz */
#define FILTRO_GAIN_UNITY    0x8000u

/* Espera el borrado del reset, carga los coeficientes de sw/common/fir_coefs.h en los
 * dos bancos y deja el filtro en bypass, banco 0, 127 taps, ganancia 1.0. */
void filtro_init(uintptr_t base);

/* Bypass (1 = la salida es la entrada con ganancia) y banco de coeficientes. */
void filtro_config(uintptr_t base, int bypass, uint32_t bank);

/* Ganancia Q1.15 sin signo (volumen, CD-05): 0x8000 = 1.0. */
void filtro_set_gain(uintptr_t base, uint32_t gain_q15);

/* Pone en cero las lineas de retardo (128 ciclos). */
void filtro_clear(uintptr_t base);

/* Par estereo empaquetado {R[31:16], L[15:0]}. */
static inline uint32_t filtro_pack(int16_t l, int16_t r)
{
    return ((uint32_t)(uint16_t)r << 16) | (uint16_t)l;
}
void     filtro_write(uintptr_t base, uint32_t lr);
uint32_t filtro_read(uintptr_t base);

/* Devuelve 1 si alguna salida saturo desde la ultima llamada, y lo borra. */
int filtro_take_sat(uintptr_t base);

/* Autoprueba (NV-15, la "rutina de prueba en software" de §3-HW p.4): impulso en los
 * dos bancos, escalon, bypass con ganancia y ruido por el modelo bit a bit de
 * sw/common/dsp.c (431 pares). Devuelve la cantidad de pares distintos (0 = el modulo
 * funciona). Deja el filtro como filtro_init(). */
uint32_t filtro_selftest(uintptr_t base);

#endif /* DRIVERS_FILTRO_H */
