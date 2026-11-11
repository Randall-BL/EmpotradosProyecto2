#include "ports_niosv.h"
#include "adc.h"
#include "audio.h"
#include "filtro.h"
#include "pio.h"
#include "seg7.h"
#include "system.h"
#include "timebase.h"

static uint32_t audio_space_port(void *ctx)
{
    (void)ctx;
    return audio_space(AUDIO_BASE);
}

static void audio_write_port(void *ctx, uint32_t lr)
{
    (void)ctx;
    filtro_write(FILTRO_BASE, lr);
    uint32_t y = filtro_read(FILTRO_BASE);
    audio_write_frame(AUDIO_BASE, (int16_t)(y & 0xFFFFu), (int16_t)(y >> 16));
}

static void fx_filter_port(void *ctx, filter_id_t f)
{
    (void)ctx;
    /* La reverberacion la hace el Nios V: el modulo queda en bypass (solo ganancia). */
    int bypass = (f == FILTER_BYPASS || f == FILTER_REVERB);
    filtro_config(FILTRO_BASE, bypass,
                  f == FILTER_BANDPASS ? FILTRO_BANK_BANDPASS : FILTRO_BANK_LOWPASS);
}

static void fx_gain_port(void *ctx, uint32_t gain_q15)
{
    (void)ctx;
    filtro_set_gain(FILTRO_BASE, gain_q15);
}

static uint32_t clock_port(void *ctx)
{
    (void)ctx;
    return timebase_ms();
}

static void panel_time(void *ctx, uint32_t s)
{
    (void)ctx;
    seg7_set_time(SEG7_BASE, s);
}

static void panel_track(void *ctx, uint32_t n)
{
    (void)ctx;
    seg7_set_track(SEG7_BASE, n, n != 0);
}

static void panel_leds(void *ctx, uint32_t v)
{
    (void)ctx;
    pio_write(PIO_LED_BASE, v);
}

static uint32_t input_sw(void *ctx)
{
    (void)ctx;
    return pio_read(PIO_SW_BASE);
}

static uint32_t input_adc(void *ctx)
{
    (void)ctx;
    return adc_read(ADC_BASE, 0);
}

static void screen_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    vga_text_print((const vga_text_t *)ctx, x, y, s, width);
}

void ports_niosv_init(app_ports_t *p, vga_text_t *vga)
{
    p->audio = (audio_port_t){ 0, AUDIO_FIFO_DEPTH, audio_space_port, audio_write_port };
    p->fx = (fx_port_t){ 0, fx_filter_port, fx_gain_port };
    p->clock = (clock_port_t){ 0, clock_port };
    p->panel = (panel_port_t){ 0, panel_time, panel_track, panel_leds };
    p->input = (input_port_t){ 0, input_sw, input_adc };
    p->screen = (screen_port_t){ vga, screen_print };
}
