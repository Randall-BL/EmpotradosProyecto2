/*
 * Prueba del driver del IP de Audio (NV-06, PR-05) para sim/questa/tb_audio.sv.
 *
 * Envia una secuencia conocida: en el par n, izquierda = n * 0x0123 (mod 2^16) y
 * derecha = ~izquierda. El testbench modela el codec WM8731 (maestro, Left
 * Justified, 16 bits, 48 kHz), deserializa AUD_DACDAT y verifica que cada par
 * llega completo, en orden y sin huecos, es decir, que la FIFO nunca se vacia.
 *
 * Etapa 1 (LEDR = 0x0A1): relleno por sondeo de fifospace.
 * Etapa 2 (LEDR = 0x0A2): relleno por la IRQ de escritura del IP (WE).
 * No espera la autoinicializacion del codec: eso lo prueba test_codec.
 */
#include <stdint.h>
#include "system.h"
#include "audio.h"
#include "irq.h"
#include "pio.h"

#define STEP        0x0123u
#define POLL_FRAMES 192u

static volatile uint16_t next_left;

/* Llena todo el espacio libre de las FIFO; devuelve cuantos pares escribio. */
static uint32_t fill(void)
{
    uint32_t n = audio_space(AUDIO_BASE);
    for (uint32_t i = 0; i < n; ++i) {
        uint16_t l = next_left;
        audio_write_frame(AUDIO_BASE, (int16_t)l, (int16_t)~l);
        next_left = (uint16_t)(l + STEP);
    }
    return n;
}

static void audio_isr(void *ctx)
{
    (void)ctx;
    (void)fill();
}

int main(void)
{
    uint32_t sent = 0;

    audio_init(AUDIO_BASE);
    next_left = 0;

    /* Etapa 1: sondeo. */
    pio_write(PIO_LED_BASE, 0x0A1);
    while (sent < POLL_FRAMES)
        sent += fill();

    /* Etapa 2: interrupcion. El lazo principal queda libre. */
    irq_attach(AUDIO_IRQ_INTERRUPT_CONTROLLER_ID, AUDIO_IRQ, audio_isr, 0);
    audio_irq_enable(AUDIO_BASE, 1);
    pio_write(PIO_LED_BASE, 0x0A2);
    for (;;) {
    }
}
