/*
 * Driver del IP de Audio del University Program (altera_up_avalon_audio), modo
 * Memory-Mapped, solo salida, 16 bits por canal (NV-06). Sin HAL.
 *
 * Mapa de registros ("Audio Core for Intel DE-Series Boards", tabla 1):
 *   0x0 control    bit0 RE, bit1 WE (IRQ de escritura), bit2 CR, bit3 CW (limpia
 *                  las FIFO de salida mientras valga 1), bit9 WI (IRQ pendiente)
 *   0x4 fifospace  [31:24] WSLC y [23:16] WSRC: palabras libres en las FIFO de
 *                  salida izquierda y derecha (128 cuando estan vacias)
 *   0x8 leftdata   escritura: muestra del canal izquierdo
 *   0xC rightdata  escritura: muestra del canal derecho
 *
 * La IRQ de escritura (si WE = 1) se activa mientras alguna FIFO tenga >= 75 %
 * libre, es decir, con 96 o mas palabras libres.
 */
#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include <stdint.h>

#define AUDIO_FIFO_DEPTH 128u

/* Limpia las FIFO de salida y deja la IRQ de escritura deshabilitada. */
void audio_init(uintptr_t base);

/* Pares de muestras (izquierda, derecha) que caben ahora en las FIFO. */
uint32_t audio_space(uintptr_t base);

/* Escribe hasta frames pares de muestras intercaladas (L, R, L, R, ...) sin
 * bloquear: solo lo que cabe. Devuelve cuantos pares escribio. */
uint32_t audio_write(uintptr_t base, const int16_t *lr, uint32_t frames);

/* Escribe un par de muestras; el llamador ya verifico el espacio. */
void audio_write_frame(uintptr_t base, int16_t left, int16_t right);

/* Habilita o deshabilita la IRQ de escritura (FIFO con >= 75 % libre). */
void audio_irq_enable(uintptr_t base, int enable);

#endif /* DRIVERS_AUDIO_H */
