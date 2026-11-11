/*
 * Prueba de integracion del lazo principal (NV-14) en el PC: app.c con la fuente real
 * sobre memoria compartida (shm_source, NV-13), el modelo del HPS del otro lado y
 * mocks de los perifericos (SC-14). El reloj es simulado: cada paso es 1 ms y la FIFO
 * del codec se vacia a 48 cuadros por ms (48 kHz), como en la tarjeta.
 *
 * Cubre IN-07 sin tarjeta: botones + FSM + playlist + avance automatico con 10
 * pistas; y lo que mide PR-07/PR-08: respuesta a botones, cambio de filtro, cambio de
 * cancion y dos vueltas a la lista sin subdesbordes.
 */
#include <string.h>
#include "app.h"
#include "hps_model.h"
#include "shm_source.h"
#include "test.h"

/* ---------- mocks ---------- */

static shm_t shm;
static hps_model_t hps;
static uint32_t hps_rate = 256;            /* cuadros que produce el HPS por ms */
static shm_source_t src;
static evq_t events;
static int16_t rev_l[APP_REVERB_FRAMES], rev_r[APP_REVERB_FRAMES];
static app_t app;

static uint32_t now_ms;
static uint32_t fifo_level;                /* cuadros en la FIFO del codec */
static uint32_t frames_out;                /* cuadros escritos por la app */
static uint32_t last_frame;
static uint32_t stream_errors;             /* cuadros fuera de orden o de otra pista */
static uint32_t exp_track = 0xFFFF, exp_pos;
static int check_stream = 1;

static uint32_t sw_value, adc_value = 4095;
static filter_id_t hw_filter = FILTER_COUNT;
static uint32_t hw_gain, filter_calls, gain_calls;
static uint32_t panel_time = 999, panel_track = 999, panel_leds;
static char grid[SCREEN_ROWS][SCREEN_COLS + 1];

#define AUDIO_DEPTH_MOCK 128u             /* FIFO del IP de Audio (NV-06) */

static uint32_t audio_space(void *ctx)
{
    (void)ctx;
    return AUDIO_DEPTH_MOCK - fifo_level;
}

static void audio_write(void *ctx, uint32_t lr)
{
    (void)ctx;
    fifo_level++;
    frames_out++;
    last_frame = lr;
    if (!check_stream)
        return;
    uint32_t track = (lr >> 16) & ~HPS_MODEL_TAG, pos = lr & 0xFFFFu;
    if (track != exp_track) {              /* pista nueva: debe empezar en 0 */
        exp_track = track;
        if (pos != 0)
            stream_errors++;
    } else if (pos != ((exp_pos + 1u) & 0xFFFFu)) {
        stream_errors++;
    }
    exp_pos = pos;
}

static void fx_filter(void *ctx, filter_id_t f) { (void)ctx; hw_filter = f; filter_calls++; }
static void fx_gain(void *ctx, uint32_t g) { (void)ctx; hw_gain = g; gain_calls++; }
static uint32_t clock_ms(void *ctx) { (void)ctx; return now_ms; }
static void panel_set_time(void *ctx, uint32_t s) { (void)ctx; panel_time = s; }
static void panel_set_track(void *ctx, uint32_t n) { (void)ctx; panel_track = n; }
static void panel_set_leds(void *ctx, uint32_t v) { (void)ctx; panel_leds = v; }
static uint32_t in_sw(void *ctx) { (void)ctx; return sw_value; }
static uint32_t in_adc(void *ctx) { (void)ctx; return adc_value; }

static void grid_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    (void)ctx;
    if (y >= SCREEN_ROWS)
        return;
    uint32_t i = 0;
    for (; s[i] && x + i < SCREEN_COLS; ++i)
        grid[y][x + i] = s[i];
    for (; i < width && x + i < SCREEN_COLS; ++i)
        grid[y][x + i] = ' ';
}

static int row_has(uint32_t y, const char *s) { return strstr(grid[y], s) != NULL; }

/* Un ms: la app da una vuelta (o varias), el HPS produce y el codec consume 48. */
static void step_ms(uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        if (hps.s)
            hps_model_poll(&hps, hps_rate);
        app_step(&app);
        app_step(&app);
        fifo_level = fifo_level > 48u ? fifo_level - 48u : 0u;
        now_ms++;
    }
}

static void press(uint32_t key)
{
    evq_push(&events, APP_EV_KEY(key));
}

static void setup(uint32_t ntracks, uint32_t frames, uint32_t bad_mask, int start_hps)
{
    memset(&shm, 0, sizeof shm);
    for (uint32_t y = 0; y < SCREEN_ROWS; ++y) {
        memset(grid[y], ' ', SCREEN_COLS);
        grid[y][SCREEN_COLS] = '\0';
    }
    now_ms = 1000;
    fifo_level = frames_out = stream_errors = 0;
    exp_track = 0xFFFF;
    check_stream = 1;
    sw_value = 0;
    adc_value = 4095;
    hps_rate = 256;
    evq_init(&events);
    shm_source_init(&src, &shm);
    app_ports_t p;
    p.audio = (audio_port_t){ NULL, AUDIO_DEPTH_MOCK, audio_space, audio_write };
    p.fx = (fx_port_t){ NULL, fx_filter, fx_gain };
    p.clock = (clock_port_t){ NULL, clock_ms };
    p.panel = (panel_port_t){ NULL, panel_set_time, panel_set_track, panel_set_leds };
    p.input = (input_port_t){ NULL, in_sw, in_adc };
    shm_source_port(&src, &p.source);
    p.screen = (screen_port_t){ NULL, grid_print };
    app_init(&app, &p, &events, rev_l, rev_r);
    if (start_hps)
        hps_model_init(&hps, &shm, ntracks, frames, bad_mask);
    else
        hps.s = NULL;
}

/* ---------- pruebas ---------- */

static void test_waiting_for_hps(void)
{
    setup(0, 0, 0, 0);
    hps.s = &shm;                          /* el modelo existe pero no publico nada */
    hps.streaming = 0;
    for (int i = 0; i < 50; ++i) {
        app_step(&app);
        now_ms++;
    }
    CHECK(!app.ready);
    CHECK(row_has(7, "Esperando al HPS"));
    CHECK(row_has(3, "esperando la lista"));
    CHECK_EQ(panel_track, 0);
    press(0);                              /* un boton sin lista no rompe nada */
    app_step(&app);
    CHECK_EQ(app_state(&app), FSM_STOPPED);

    hps_model_init(&hps, &shm, 12, 4800, 0);
    step_ms(150);
    CHECK(app.ready);
    CHECK_EQ(app.pl.count, 12);
    CHECK_EQ(panel_track, 1);
    CHECK(row_has(3, "Cancion 01"));
    CHECK(row_has(1, " 1 / 12"));
    CHECK(!row_has(7, "Esperando"));
}

static void test_play_pause_stop(void)
{
    setup(10, 3u * 48000u, 0, 1);
    step_ms(120);
    CHECK_EQ(panel_leds & (APP_LED_PLAYING | APP_LED_PAUSED), 0);

    uint32_t t0 = now_ms;
    press(0);
    step_ms(1);
    CHECK_EQ(app_state(&app), FSM_PLAYING);
    CHECK(panel_leds & APP_LED_PLAYING);
    CHECK(now_ms - t0 < 200);              /* respuesta a botones (PR-07) */

    step_ms(1500);
    CHECK(frames_out > 1400u * 48u);
    CHECK_EQ(panel_time, 1);               /* 1.5 s -> 1 s en el 7 segmentos */
    CHECK_EQ(app.elapsed_s, 1);
    step_ms(150);
    CHECK(row_has(6, "Transcurrido: 00:01"));
    CHECK(row_has(8, "REPRODUCIENDO"));

    press(0);                              /* pausa: no sale audio ni avanza el tiempo */
    step_ms(5);
    uint32_t frozen = frames_out, el = app.elapsed_s;
    step_ms(2000);
    CHECK_EQ(frames_out, frozen);
    CHECK_EQ(app.elapsed_s, el);
    CHECK(panel_leds & APP_LED_PAUSED);
    CHECK(row_has(8, "PAUSADO"));

    press(0);                              /* reanuda donde iba, sin recargar */
    step_ms(600);
    CHECK_EQ(app.elapsed_s, 2);
    CHECK_EQ(hps.commands, 1);

    press(1);                              /* stop: tiempo a cero, misma pista */
    step_ms(150);
    CHECK_EQ(app_state(&app), FSM_STOPPED);
    CHECK_EQ(panel_time, 0);
    CHECK_EQ(app.elapsed_s, 0);
    CHECK_EQ(panel_track, 1);
    CHECK(row_has(8, "DETENIDO"));
    CHECK_EQ(stream_errors, 0);
    CHECK_EQ(app.underruns, 0);
}

static void test_navigation(void)
{
    setup(10, 2u * 48000u, 0, 1);
    step_ms(120);
    press(2);                              /* anterior desde la 1: la 10 */
    step_ms(60);
    CHECK_EQ(panel_track, 10);
    press(3);
    step_ms(60);
    CHECK_EQ(panel_track, 1);
    CHECK_EQ(app_state(&app), FSM_STOPPED);
    CHECK_EQ(hps.commands, 0);             /* detenido: solo cambia la seleccion */

    press(0);
    step_ms(500);
    /* Cambio de cancion reproduciendo: el audio nuevo empieza en < 2 s (PR-07) y no
     * se cuela nada de la pista anterior (lo verifica audio_write). */
    uint32_t t0 = now_ms;
    press(3);
    while (exp_track != 1u && now_ms - t0 < 3000)
        step_ms(1);
    CHECK_EQ(exp_track, 1);
    CHECK(now_ms - t0 < 2000);
    CHECK_EQ(panel_track, 2);
    step_ms(200);
    CHECK(row_has(3, "Cancion 02"));

    /* Rebote: dos flancos del mismo boton en la ventana -> una sola accion. */
    press(3);
    press(3);
    step_ms(1);
    CHECK_EQ(panel_track, 3);
    CHECK_EQ(app.deb.rejected, 1);
    step_ms(DEB_WINDOW_MS);
    press(2);
    step_ms(100);
    CHECK_EQ(panel_track, 2);

    /* Cambio en pausa: carga la otra pista pero no suena hasta reanudar. */
    press(0);
    step_ms(60);
    uint32_t frozen = frames_out;
    press(3);
    step_ms(300);
    CHECK_EQ(frames_out, frozen);
    CHECK_EQ(app_state(&app), FSM_PAUSED);
    press(0);
    step_ms(100);
    CHECK_EQ(exp_track, 2);
    CHECK_EQ(stream_errors, 0);
    CHECK_EQ(app.keys_accepted, 9);       /* 10 flancos, 1 rebote */
}

/* IN-07 / PR-08: dos vueltas completas a 10 pistas con avance automatico. */
static void test_two_laps(void)
{
    const uint32_t frames = 9600;          /* 0.2 s por pista */
    setup(10, frames, 0, 1);
    step_ms(120);
    press(0);
    uint32_t seen[10] = { 0 };
    uint32_t last = 0xFFFF;
    for (int i = 0; i < 5000 && app.laps < 2; ++i) {
        step_ms(1);
        if (exp_track != last && exp_track < 10) {
            seen[exp_track]++;
            last = exp_track;
        }
    }
    CHECK_EQ(app.laps, 2);
    CHECK_EQ(app.tracks_played, 20);
    for (int i = 0; i < 10; ++i)
        CHECK(seen[i] >= 2);
    CHECK_EQ(stream_errors, 0);
    CHECK_EQ(app.underruns, 0);
    CHECK_EQ(app_state(&app), FSM_PLAYING);
    CHECK_EQ(panel_leds & APP_LED_UNDERRUN, 0);
    /* 20 pistas de 9600 cuadros (mas lo que va de la actual). */
    CHECK(frames_out >= 20u * frames);
}

static void test_bad_tracks(void)
{
    setup(10, 4800, (1u << 2) | (1u << 3), 1);
    step_ms(120);
    press(0);
    for (int i = 0; i < 2000 && exp_track != 4u; ++i)
        step_ms(1);
    CHECK_EQ(exp_track, 4);                /* 2 y 3 se saltaron */
    CHECK_EQ(stream_errors, 0);

    setup(3, 4800, 0x7, 1);                /* ninguna se puede leer */
    step_ms(120);
    press(0);
    step_ms(200);
    CHECK_EQ(app_state(&app), FSM_STOPPED);
    CHECK(app.all_failed);
    CHECK(panel_leds & APP_LED_ALERT);
    CHECK(row_has(7, "ninguna cancion"));
}

static void test_hps_failed(void)
{
    setup(0, 0, 0, 0);
    shm_hps_init(&shm);
    shm.ctrl.hps_state = SHM_HPS_NO_SD;
    step_ms(150);
    CHECK(!app.ready);
    CHECK(panel_leds & APP_LED_ALERT);
    CHECK(row_has(7, "no encontro la SD"));

    setup(9, 4800, 0, 1);                  /* menos de 10: funciona pero avisa */
    step_ms(150);
    CHECK(app.ready);
    CHECK(row_has(7, "menos de 10"));
}

static void test_underrun(void)
{
    setup(10, 10u * 48000u, 0, 1);
    step_ms(120);
    press(0);
    step_ms(500);
    CHECK_EQ(app.underruns, 0);
    hps_rate = 0;                          /* la SD se atasca */
    step_ms(400);
    CHECK_EQ(app.underruns, 1);            /* un episodio cuenta una vez */
    CHECK(panel_leds & APP_LED_UNDERRUN);
    hps_rate = 256;
    step_ms(300);
    hps_rate = 0;
    step_ms(400);
    CHECK_EQ(app.underruns, 2);
    hps_rate = 256;
    step_ms(200);
    CHECK(row_has(7, "Subdesbordes: 2"));
    CHECK_EQ(stream_errors, 0);            /* se corto, pero no se perdieron cuadros */
}

/* Switches -> filtro en < 100 ms (PR-07) y potenciometro -> ganancia. */
static void test_filters_and_volume(void)
{
    setup(10, 10u * 48000u, 0, 1);
    step_ms(120);
    CHECK_EQ(hw_filter, FILTER_BYPASS);
    press(0);
    step_ms(200);

    static const struct { uint32_t sw; filter_id_t f; uint32_t led; } cases[] = {
        { 0x1, FILTER_LOWPASS, 1 }, { 0x2, FILTER_BANDPASS, 2 },
        { 0x4, FILTER_REVERB, 4 }, { 0x0, FILTER_BYPASS, 0 },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        uint32_t t0 = now_ms;
        sw_value = cases[i].sw;
        while (hw_filter != cases[i].f && now_ms - t0 < 500)
            step_ms(1);
        CHECK_EQ(hw_filter, cases[i].f);
        CHECK(now_ms - t0 <= 2u * SW_POLL_MS + 1u);
        CHECK(now_ms - t0 < 100);
        CHECK_EQ(panel_leds & APP_LED_FILTER, cases[i].led);
        step_ms(200);
    }
    CHECK(row_has(8, "BYPASS"));

    /* Reverberacion: el audio que sale ya no es el del ring (lo procesa el Nios V). */
    check_stream = 0;
    sw_value = 0x4;
    step_ms(100);
    uint32_t raw = 0, total = 0;
    for (int i = 0; i < 50; ++i) {
        step_ms(1);
        total++;
        if ((last_frame >> 16) == (HPS_MODEL_TAG | 0u))
            raw++;
    }
    CHECK(raw < total);

    sw_value = 0x5;                        /* invalida */
    step_ms(100);
    CHECK_EQ(hw_filter, FILTER_BYPASS);
    CHECK(panel_leds & APP_LED_ALERT);
    step_ms(100);
    CHECK(row_has(8, "SW INVALIDO"));
    sw_value = 0;
    step_ms(100);
    CHECK_EQ(panel_leds & APP_LED_ALERT, 0);

    uint32_t g0 = hw_gain, calls = gain_calls;
    adc_value = 1000;
    step_ms(60);
    CHECK(gain_calls > calls);
    CHECK(hw_gain < g0);
    adc_value = 1010;                      /* dentro de la histeresis: nada */
    calls = gain_calls;
    step_ms(60);
    CHECK_EQ(gain_calls, calls);
}

/* El LED de botones cambia con cada pulsacion aceptada (metodo de PR-07). */
static void test_key_led(void)
{
    setup(10, 48000, 0, 1);
    step_ms(120);
    uint32_t before = panel_leds & APP_LED_KEY;
    press(3);
    step_ms(1);
    CHECK((panel_leds & APP_LED_KEY) != before);
    step_ms(DEB_WINDOW_MS);
    press(2);
    step_ms(1);
    CHECK_EQ(panel_leds & APP_LED_KEY, before);
    press(7);                              /* evento desconocido: se ignora */
    step_ms(1);
    CHECK_EQ(app.keys_accepted, 2);
}

int main(void)
{
    RUN(test_waiting_for_hps);
    RUN(test_play_pause_stop);
    RUN(test_navigation);
    RUN(test_two_laps);
    RUN(test_bad_tracks);
    RUN(test_hps_failed);
    RUN(test_underrun);
    RUN(test_filters_and_volume);
    RUN(test_key_led);
    return TEST_REPORT();
}
