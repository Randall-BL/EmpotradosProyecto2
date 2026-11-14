/*
 * Conversion de PCM de 16 bits a cuadros estereo de 48 kHz para el ring (HP-07, CD-07).
 *
 * El codec corre fijo a 48 kHz (CD-07), asi que el HPS convierte cada WAV antes de
 * escribirlo en shm: 8 kHz (x6), 16 kHz (x3), 44.1 kHz (x160/147) y 48 kHz (copia).
 * Mono se duplica en los dos canales. La salida va empaquetada como filtro_pack():
 * izquierdo en los 16 bits bajos y derecho en los altos.
 *
 * Interpolacion lineal entre muestras vecinas con la fase exacta en enteros
 * (fase += in_rate; cada 48 000 se avanza una muestra de entrada): no se acumula
 * deriva y no hace falta punto flotante. La lineal atenua un poco los agudos al
 * subir de 8 o 16 kHz; para 44.1 -> 48 kHz el error es inaudible.
 *
 * Trabaja por bloques: consume la entrada que pueda mientras quede espacio en la
 * salida y conserva el estado entre llamadas, asi el productor escribe en el ring
 * solo el espacio libre que haya. Portable: sin malloc ni dependencias.
 */
#ifndef COMMON_RESAMPLE_H
#define COMMON_RESAMPLE_H

#include <stdint.h>

#define RESAMPLE_OUT_RATE 48000u

typedef struct {
    uint32_t in_rate;                     /* Hz del archivo */
    uint32_t phase;                       /* 0..RESAMPLE_OUT_RATE-1 entre a y b */
    int16_t a[2];                         /* muestra de entrada anterior (L, R) */
    int has_a;                            /* 0 hasta recibir la primera muestra */
} resample_t;

/* Prepara la conversion desde in_rate Hz (1..RESAMPLE_OUT_RATE). */
void resample_init(resample_t *r, uint32_t in_rate);

/*
 * Convierte cuadros de entrada intercalados (channels = 1 o 2) a cuadros de 48 kHz.
 * Consume como mucho in_frames cuadros y produce como mucho out_max. Devuelve los
 * cuadros producidos y deja en *consumed los consumidos: la entrada no consumida se
 * vuelve a pasar en la siguiente llamada.
 */
uint32_t resample_run(resample_t *r, const int16_t *in, uint32_t in_frames,
                      uint32_t channels, uint32_t *consumed,
                      uint32_t *out, uint32_t out_max);

/* Cuadros de 48 kHz que salen de in_frames cuadros a in_rate (para la tabla de pistas). */
uint32_t resample_out_frames(uint32_t in_frames, uint32_t in_rate);

/* Empaqueta un cuadro estereo como lo espera el ring: L en [15:0], R en [31:16]. */
static inline uint32_t resample_pack(int16_t l, int16_t r)
{
    return (uint32_t)(uint16_t)l | ((uint32_t)(uint16_t)r << 16);
}

#endif /* COMMON_RESAMPLE_H */
