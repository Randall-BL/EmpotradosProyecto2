/*
 * Puertos de la aplicacion implementados con los drivers del Nios V (SC-14). Ver
 * ports_niosv.h. Cada funcion es un adaptador de la firma generica de ports.h a un
 * driver concreto; ctx no se usa salvo en la pantalla (las bases salen de system.h).
 */
#include "ports_niosv.h"
#include "adc.h"
#include "audio.h"
#include "filtro.h"
#include "pio.h"
#include "seg7.h"
#include "system.h"
#include "timebase.h"

/* audio.space: pares que caben en la FIFO del codec. */
static uint32_t audio_space_port(void *ctx)
{
    (void)ctx;
    return audio_space(AUDIO_BASE);
}

/* audio.write: el par pasa primero por el modulo propio (FIR + ganancia) y el
 * resultado se escribe en la FIFO del codec, separado en L (bits bajos) y R. */
static void audio_write_port(void *ctx, uint32_t lr)
{
    (void)ctx;
    filtro_write(FILTRO_BASE, lr);
    uint32_t y = filtro_read(FILTRO_BASE);
    audio_write_frame(AUDIO_BASE, (int16_t)(y & 0xFFFFu), (int16_t)(y >> 16));
}

/* fx.set_filter: traduce el filtro elegido a bypass + banco del modulo. */
static void fx_filter_port(void *ctx, filter_id_t f)
{
    (void)ctx;
    /* La reverberacion la hace el Nios V: el modulo queda en bypass (solo ganancia). */
    int bypass = (f == FILTER_BYPASS || f == FILTER_REVERB);
    filtro_config(FILTRO_BASE, bypass,
                  f == FILTER_BANDPASS ? FILTRO_BANK_BANDPASS : FILTRO_BANK_LOWPASS);
}

/* fx.set_gain: volumen en el registro de ganancia del modulo. */
static void fx_gain_port(void *ctx, uint32_t gain_q15)
{
    (void)ctx;
    filtro_set_gain(FILTRO_BASE, gain_q15);
}

/* clock.now_ms: milisegundos desde el arranque (mtime). */
static uint32_t clock_port(void *ctx)
{
    (void)ctx;
    return timebase_ms();
}

/* panel.set_time: segundos transcurridos al 7 segmentos (HEX3..HEX0). */
static void panel_time(void *ctx, uint32_t s)
{
    (void)ctx;
    seg7_set_time(SEG7_BASE, s);
}

/* panel.set_track: numero de pista en HEX5..HEX4; 0 apaga esos digitos. */
static void panel_track(void *ctx, uint32_t n)
{
    (void)ctx;
    seg7_set_track(SEG7_BASE, n, n != 0);
}

/* panel.set_leds: valor completo de LEDR[9:0]. */
static void panel_leds(void *ctx, uint32_t v)
{
    (void)ctx;
    pio_write(PIO_LED_BASE, v);
}

/* input.switches: posicion de SW[9:0]. */
static uint32_t input_sw(void *ctx)
{
    (void)ctx;
    return pio_read(PIO_SW_BASE);
}

/* input.volume_adc: canal 0 del ADC, donde esta conectado el potenciometro. */
static uint32_t input_adc(void *ctx)
{
    (void)ctx;
    return adc_read(ADC_BASE, 0);
}

/* screen.print: texto al Character Buffer (ctx = vga_text_t). */
static void screen_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    vga_text_print((const vga_text_t *)ctx, x, y, s, width);
}

/* Llena todos los puertos menos la fuente de audio (que la agrega main.c con
 * shm_source_port). */
void ports_niosv_init(app_ports_t *p, vga_text_t *vga)
{
    p->audio = (audio_port_t){ 0, AUDIO_FIFO_DEPTH, audio_space_port, audio_write_port };
    p->fx = (fx_port_t){ 0, fx_filter_port, fx_gain_port };
    p->clock = (clock_port_t){ 0, clock_port };
    p->panel = (panel_port_t){ 0, panel_time, panel_track, panel_leds };
    p->input = (input_port_t){ 0, input_sw, input_adc };
    p->screen = (screen_port_t){ vga, screen_print };
}
