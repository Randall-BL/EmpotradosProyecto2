/*
 * Prueba integrada en la tarjeta de la plataforma de A (IN-02, IN-03, IN-04,
 * PR-05): tono senoidal por el line-out, volumen con el potenciometro y pantalla
 * VGA con metadatos fijos. Procedimiento y resultados esperados en
 * docs/pruebas-tarjeta.md.
 *
 *   SW0 = 0: 440 Hz · SW0 = 1: 1 kHz        KEY0: silencio / sonido
 *   LEDR[9:0]: barra con el escalon de volumen
 *
 * Todo con los drivers sin HAL; el volumen se aplica en software (SC-11).
 */
#include <stdint.h>
#include "system.h"
#include "adc.h"
#include "audio.h"
#include "codec.h"
#include "pio.h"
#include "screen.h"
#include "tone.h"
#include "vga_text.h"
#include "volume.h"

#define FS             48000u
#define CODEC_TIMEOUT  20000000u
#define POT_CHANNEL    0u

static vga_text_t vga;

static void vga_port_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    vga_text_print((const vga_text_t *)ctx, x, y, s, width);
}

/* Barra de 10 LEDs para el escalon de volumen (0..31): un LED cada ~3 escalones. */
static uint32_t led_bar(uint32_t step)
{
    uint32_t n = 0;
    for (uint32_t s = step; s >= 3u && n < 10u; s -= 3u)
        ++n;
    return (1u << n) - 1u;
}

int main(void)
{
    vga_text_init(&vga, CHAR_BUF_AVALON_CHAR_CONTROL_SLAVE_BASE,
                  CHAR_BUF_AVALON_CHAR_BUFFER_SLAVE_BASE);
    screen_port_t port = { &vga, vga_port_print };
    screen_track_t info = { "Tono de prueba 440 Hz", "Plataforma (persona A)",
                            "IN-02 / IN-03 / IN-04", 0, 1, 1 };
    screen_draw_frame(&port);
    screen_draw_track(&port, &info);

    int rc = codec_init(AV_CONFIG_BASE, CODEC_TIMEOUT);
    if (rc != CODEC_OK) {
        vga_text_print(&vga, 2, 8, "ERROR: el codec no respondio (I2C)", 76);
        pio_write(PIO_LED_BASE, 0x3FF);
        for (;;) {
        }
    }

    adc_init(ADC_BASE);
    volume_t vol;
    volume_init(&vol, adc_read(ADC_BASE, POT_CHANNEL));

    tone_t tone;
    uint32_t sw0 = pio_read(PIO_SW_BASE) & 1u;
    tone_init(&tone, sw0 ? 1000u : 440u, FS);
    audio_init(AUDIO_BASE);
    (void)pio_edges_take(PIO_KEY_BASE);

    int muted = 0;
    uint32_t frames = 0, redraw = 1;
    for (;;) {
        /* Audio: llenar lo que quepa en la FIFO. */
        uint32_t n = audio_space(AUDIO_BASE);
        for (uint32_t i = 0; i < n; ++i) {
            int16_t s = muted ? 0 : volume_scale(tone_next(&tone), vol.gain_q15);
            audio_write_frame(AUDIO_BASE, s, s);
        }
        frames += n;

        /* Controles y pantalla, sin apuro (unas 50 veces por segundo). */
        if (frames >= FS / 50u) {
            frames -= FS / 50u;
            if (volume_update(&vol, adc_read(ADC_BASE, POT_CHANNEL)))
                redraw = 1;
            uint32_t sw = pio_read(PIO_SW_BASE) & 1u;
            if (sw != sw0) {
                sw0 = sw;
                tone_init(&tone, sw0 ? 1000u : 440u, FS);
                info.title = sw0 ? "Tono de prueba 1 kHz" : "Tono de prueba 440 Hz";
                screen_draw_track(&port, &info);
            }
            if (pio_edges_take(PIO_KEY_BASE) & 1u) {
                muted = !muted;
                redraw = 1;
            }
            if (redraw) {
                screen_draw_status(&port, muted ? SCREEN_PAUSED : SCREEN_PLAYING,
                                   sw0 ? "TONO 1 kHz (SW0)" : "TONO 440 Hz (SW0)",
                                   volume_percent(&vol), 0);
                pio_write(PIO_LED_BASE, led_bar(vol.step));
                redraw = 0;
            }
        }
    }
}
