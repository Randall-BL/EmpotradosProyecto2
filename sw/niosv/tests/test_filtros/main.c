/*
 * Prueba en la tarjeta de los tres filtros seleccionables con switches (IN-08,
 * PR-09/PR-10 a oido) mientras suena audio. Procedimiento: docs/pruebas-tarjeta.md.
 *
 * Fuente: mezcla de tres tonos (300 Hz + 2.5 kHz + 8 kHz) para que cada filtro se
 * distinga: el pasa-bajos deja solo el grave, el pasa-banda el medio, el bypass los
 * tres, y la reverberacion agrega ecos (se oye mejor con KEY1, que corta y vuelve).
 *
 *   SW[1:0] = 00 bypass · 01 pasa-bajos 1 kHz (HW) · 10 pasa-banda 1-4 kHz (HW)
 *           · 11 reverberacion (SW, Nios V)          (asignacion provisional; CD-08 de C)
 *   Potenciometro (ADC CH0): volumen -> GAIN del modulo propio (CD-05)
 *   KEY0: silencio/sonido · KEY1: pulsos de 0.2 s (para oir la reverberacion)
 *   HEX3..HEX0: tiempo MM:SS · VGA: filtro, estado y volumen · LEDR[1:0]: filtro
 */
#include <stdint.h>
#include "system.h"
#include "adc.h"
#include "audio.h"
#include "codec.h"
#include "dsp.h"
#include "filtro.h"
#include "pio.h"
#include "screen.h"
#include "seg7.h"
#include "tone.h"
#include "vga_text.h"
#include "volume.h"

#define FS            48000u
#define REVERB_D      2400u                 /* 50 ms */
#define CODEC_TIMEOUT 20000000u

static vga_text_t vga;
static int16_t rev_buf_l[REVERB_D], rev_buf_r[REVERB_D];

static void vga_port_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    vga_text_print((const vga_text_t *)ctx, x, y, s, width);
}

static const char *const NAMES[4] = {
    "BYPASS (SW=00)", "PASA-BAJOS 1 kHz (SW=01)", "PASA-BANDA 1-4 kHz (SW=10)", "REVERB (SW=11)",
};

int main(void)
{
    vga_text_init(&vga, CHAR_BUF_AVALON_CHAR_CONTROL_SLAVE_BASE, CHAR_BUF_AVALON_CHAR_BUFFER_SLAVE_BASE);
    screen_port_t port = { &vga, vga_port_print };
    screen_track_t info = { "Mezcla de tonos 300 Hz + 2.5 kHz + 8 kHz", "Filtros (persona B)",
                            "IN-08 / PR-10", 0, 1, 1 };
    screen_draw_frame(&port);
    screen_draw_track(&port, &info);
    seg7_init(SEG7_BASE);

    if (codec_init(AV_CONFIG_BASE, CODEC_TIMEOUT) != CODEC_OK) {
        vga_text_print(&vga, 2, 8, "ERROR: el codec no respondio (I2C)", 76);
        pio_write(PIO_LED_BASE, 0x3FF);
        for (;;) {
        }
    }
    filtro_init(FILTRO_BASE);
    uint32_t bad = filtro_selftest(FILTRO_BASE);
    if (bad)
        vga_text_print(&vga, 2, 7, "AUTOPRUEBA DEL FILTRO: FALLO", 76);
    adc_init(ADC_BASE);
    audio_init(AUDIO_BASE);

    tone_t t1, t2, t3;
    tone_init(&t1, 300, FS);
    tone_init(&t2, 2500, FS);
    tone_init(&t3, 8000, FS);
    dsp_reverb_t rl, rr;
    dsp_reverb_init(&rl, rev_buf_l, REVERB_D);
    dsp_reverb_init(&rr, rev_buf_r, REVERB_D);
    volume_t vol;
    volume_init(&vol, adc_read(ADC_BASE, 0));
    filtro_set_gain(FILTRO_BASE, vol.gain_q15);
    (void)pio_edges_take(PIO_KEY_BASE);

    uint32_t sel = 4, frames = 0, tick = 0, seconds = 0, redraw = 1, burst = 0;
    int muted = 0, pulses = 0;
    for (;;) {
        uint32_t n = audio_space(AUDIO_BASE);
        for (uint32_t i = 0; i < n; ++i) {
            int16_t x = (int16_t)((tone_next(&t1) >> 2) + (tone_next(&t2) >> 2) + (tone_next(&t3) >> 2));
            if (muted || (pulses && burst >= FS / 5u))
                x = 0;
            if (++burst == FS)
                burst = 0;
            int16_t l = x, r = x;
            if (sel == 3) {
                l = dsp_reverb_step(&rl, l);
                r = dsp_reverb_step(&rr, r);
            }
            filtro_write(FILTRO_BASE, filtro_pack(l, r));
            uint32_t y = filtro_read(FILTRO_BASE);
            audio_write_frame(AUDIO_BASE, (int16_t)(y & 0xFFFFu), (int16_t)(y >> 16));
        }
        frames += n;
        tick += n;

        if (tick >= FS / 50u) {                     /* controles ~50 veces por segundo */
            tick -= FS / 50u;
            uint32_t s = pio_read(PIO_SW_BASE) & 3u;
            if (s != sel) {
                sel = s;
                filtro_config(FILTRO_BASE, sel == 0 || sel == 3, sel == 2 ? FILTRO_BANK_BANDPASS
                                                                          : FILTRO_BANK_LOWPASS);
                redraw = 1;
            }
            if (volume_update(&vol, adc_read(ADC_BASE, 0))) {
                filtro_set_gain(FILTRO_BASE, vol.gain_q15);
                redraw = 1;
            }
            uint32_t k = pio_edges_take(PIO_KEY_BASE);
            if (k & 1u) { muted = !muted; redraw = 1; }
            if (k & 2u) { pulses = !pulses; }
            if (frames >= FS) {
                frames -= FS;
                seg7_set_time(SEG7_BASE, ++seconds);
                redraw = 1;
            }
            if (redraw) {
                screen_draw_status(&port, muted ? SCREEN_PAUSED : SCREEN_PLAYING, NAMES[sel],
                                   volume_percent(&vol), seconds);
                pio_write(PIO_LED_BASE, sel | (bad ? 0x200u : 0u));
                redraw = 0;
            }
        }
    }
}
