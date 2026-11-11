/*
 * Co-simulacion del reproductor completo en el Nios V (IN-07 sin tarjeta, NV-13,
 * NV-14) para sim/questa/tb_player.sv.
 *
 * Corre la MISMA logica que la aplicacion (sw/common/src/app.c) con los drivers
 * reales de botones (IRQ, NV-03), switches, LEDs, 7 segmentos, pantalla, filtro y
 * base de tiempo, y la fuente real sobre la memoria compartida (shm_source, NV-13).
 * Lo que no hay en simulacion se reemplaza:
 *   - HPS: el modelo hps_model escribe en la shm real (10 pistas cortas sinteticas).
 *   - Codec: un sumidero que acepta 32 cuadros por vuelta y verifica que llegan en
 *     orden y de la pista correcta (sin restos de la anterior).
 *   - Potenciometro: fijo al maximo.
 *   - Pantalla: un puerto nulo. La pantalla ya la verifica tb_vga, y vga_text_init
 *     espera el borrado del Character Buffer (16-33 ms simulados, ~30 min de Questa).
 *   - Reloj: 100 veces mas rapido (500 ciclos por "ms") para que el antirrebote y la
 *     encuesta de switches quepan en pocos ms simulados.
 *
 * Si el sumidero detecta un cuadro fuera de lugar, LEDR = 0x3EE y se detiene.
 */
#include <stdint.h>
#include "system.h"
#include "app.h"
#include "evq.h"
#include "hps_model.h"
#include "keys.h"
#include "pio.h"
#include "ports_niosv.h"
#include "seg7.h"
#include "shm_source.h"
#include "timebase.h"

#define TRACKS        10u
#define TRACK_FRAMES  48u
#define SINK_CHUNK    32u
#define FAST_TICKS_MS 500u
#define LED_SINK_FAIL 0x3EEu

static app_t app;
static evq_t events;
static shm_source_t source;
static hps_model_t hps;
static int16_t rev_l[APP_REVERB_FRAMES], rev_r[APP_REVERB_FRAMES];

static uint32_t exp_track = 0xFFFFu, exp_pos;
static volatile uint32_t sink_errors;

static uint32_t sink_space(void *ctx)
{
    (void)ctx;
    return SINK_CHUNK;
}

static void sink_write(void *ctx, uint32_t lr)
{
    (void)ctx;
    uint32_t track = (lr >> 16) & ~HPS_MODEL_TAG, pos = lr & 0xFFFFu;
    if (track != exp_track) {
        exp_track = track;
        if (pos != 0)
            sink_errors++;
    } else if (pos != exp_pos + 1u) {
        sink_errors++;
    }
    exp_pos = pos;
}

static void null_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)s;
    (void)width;
}

static uint32_t adc_max(void *ctx)
{
    (void)ctx;
    return 4095u;
}

int main(void)
{
    pio_write(PIO_LED_BASE, 0);
    timebase_init(FAST_TICKS_MS);
    seg7_init(SEG7_BASE);

    evq_init(&events);
    keys_init(PIO_KEY_BASE, PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID, PIO_KEY_IRQ, &events);

    volatile shm_t *shm = (volatile shm_t *)SHM_BASE;
    shm->ctrl.magic = 0;                         /* el "HPS" todavia no arranco */
    shm_source_init(&source, shm);
    app_ports_t ports;
    ports_niosv_init(&ports, 0);
    ports.screen = (screen_port_t){ 0, null_print };
    ports.audio = (audio_port_t){ 0, 128u, sink_space, sink_write };
    ports.input.volume_adc = adc_max;
    shm_source_port(&source, &ports.source);
    app_init(&app, &ports, &events, rev_l, rev_r);

    /* Unas vueltas "esperando al HPS" antes de publicar la tabla. */
    for (uint32_t i = 0; i < 20u; ++i)
        app_step(&app);
    hps_model_init(&hps, shm, TRACKS, TRACK_FRAMES, 0);

    for (;;) {
        hps_model_poll(&hps, 64u);
        app_step(&app);
        if (sink_errors) {
            pio_write(PIO_LED_BASE, LED_SINK_FAIL);
            for (;;) {
            }
        }
    }
}
