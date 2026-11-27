/* Driver del IP de Audio del University Program (NV-06). Ver audio.h para el mapa de
 * registros. Solo se usa el lado de salida (DAC). */
#include "audio.h"
#include "hw.h"

/* Desplazamientos de los registros. */
#define AUDIO_REG_CONTROL   0x0   /* control de IRQ y borrado de FIFO */
#define AUDIO_REG_FIFOSPACE 0x4   /* espacio libre/ocupado en las FIFO */
#define AUDIO_REG_LEFT      0x8   /* dato del canal izquierdo */
#define AUDIO_REG_RIGHT     0xC   /* dato del canal derecho */

/* Bits del registro de control. */
#define AUDIO_CTRL_WE (1u << 1)   /* habilita la IRQ de escritura */
#define AUDIO_CTRL_CW (1u << 3)   /* borra las FIFO de salida mientras vale 1 */

/* Vacia las FIFO de salida (descarta audio viejo) y deja la IRQ apagada. */
void audio_init(uintptr_t base)
{
    /* CW limpia las FIFO de salida mientras vale 1: se sube y se baja. */
    HW_REG(base, AUDIO_REG_CONTROL) = AUDIO_CTRL_CW;
    HW_REG(base, AUDIO_REG_CONTROL) = 0;
}

/* Cuantos pares (izquierda, derecha) se pueden escribir ahora sin desbordar: el menor
 * de los espacios libres de las dos FIFO (WSLC y WSRC). */
uint32_t audio_space(uintptr_t base)
{
    uint32_t fs = HW_REG(base, AUDIO_REG_FIFOSPACE);
    uint32_t wslc = (fs >> 24) & 0xFFu;   /* libres en la FIFO izquierda */
    uint32_t wsrc = (fs >> 16) & 0xFFu;   /* libres en la FIFO derecha */
    /* Las dos FIFO avanzan juntas, pero se toma la menor por seguridad. */
    return wslc < wsrc ? wslc : wsrc;
}

/* Escribe un par de muestras, una en cada FIFO. No revisa el espacio: si la FIFO esta
 * llena el IP descarta la muestra. */
void audio_write_frame(uintptr_t base, int16_t left, int16_t right)
{
    /* El IP toma los 16 bits bajos de cada registro (dato justificado a la derecha). */
    HW_REG(base, AUDIO_REG_LEFT)  = (uint32_t)(uint16_t)left;
    HW_REG(base, AUDIO_REG_RIGHT) = (uint32_t)(uint16_t)right;
}

/* Escribe pares intercalados L, R, L, R... sin bloquear: solo los que caben ahora.
 * Devuelve cuantos pares escribio (el resto queda para la siguiente llamada). */
uint32_t audio_write(uintptr_t base, const int16_t *lr, uint32_t frames)
{
    uint32_t n = audio_space(base);
    if (n > frames)
        n = frames;
    for (uint32_t i = 0; i < n; ++i) {
        audio_write_frame(base, lr[0], lr[1]);
        lr += 2;
    }
    return n;
}

/* Enciende o apaga la IRQ de escritura (avisa cuando la FIFO tiene >= 75 % libre). */
void audio_irq_enable(uintptr_t base, int enable)
{
    HW_REG(base, AUDIO_REG_CONTROL) = enable ? AUDIO_CTRL_WE : 0;
}
