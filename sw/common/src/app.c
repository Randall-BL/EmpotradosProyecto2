#include "app.h"

/* KEY0..KEY3 -> evento de la FSM (la ayuda de la pantalla dice lo mismo). */
static const fsm_event_t KEY_EVENT[4] = {
    FSM_EV_PLAY_PAUSE, FSM_EV_STOP, FSM_EV_PREV, FSM_EV_NEXT,
};

/* ---------- utilidades sin printf (el Nios V/m no tiene divisor) ---------- */

static uint32_t put_str(char *dst, uint32_t pos, uint32_t max, const char *s)
{
    while (*s && pos + 1u < max)
        dst[pos++] = *s++;
    dst[pos] = '\0';
    return pos;
}

static uint32_t put_u32(char *dst, uint32_t pos, uint32_t max, uint32_t v)
{
    static const uint32_t pow10[] = { 1000000000u, 100000000u, 10000000u, 1000000u,
                                      100000u, 10000u, 1000u, 100u, 10u, 1u };
    int started = 0;
    for (uint32_t i = 0; i < 10u; ++i) {
        char d = '0';
        while (v >= pow10[i]) {
            v -= pow10[i];
            d++;
        }
        if (d != '0' || started || i == 9u) {
            started = 1;
            if (pos + 1u < max)
                dst[pos++] = d;
        }
    }
    dst[pos] = '\0';
    return pos;
}

static void reverb_clear(app_t *a)
{
    for (uint32_t i = 0; i < APP_REVERB_FRAMES; ++i) {
        a->rev_l.buf[i] = 0;
        a->rev_r.buf[i] = 0;
    }
    a->rev_l.pos = a->rev_r.pos = 0;
}

static void time_reset(app_t *a)
{
    a->frames_in_sec = 0;
    a->elapsed_s = 0;
    a->track_frames = 0;
    a->starving = 0;
    a->p.panel.set_time(a->p.panel.ctx, 0);
    a->dirty |= APP_DIRTY_STATUS;
}

/* ---------- LEDs y pantalla ---------- */

static uint32_t leds_value(const app_t *a)
{
    static const uint32_t FILTER_LED[FILTER_COUNT] = { 0u, 1u, 2u, 4u };
    uint32_t v = FILTER_LED[a->sw.filter] | a->key_toggle;
    if (a->underruns)
        v |= APP_LED_UNDERRUN;
    if (a->fsm.state == FSM_PLAYING)
        v |= APP_LED_PLAYING;
    else if (a->fsm.state == FSM_PAUSED)
        v |= APP_LED_PAUSED;
    if (a->sw.invalid || a->all_failed || a->src_state == SRC_FAILED)
        v |= APP_LED_ALERT;
    return v;
}

static void leds_refresh(app_t *a)
{
    uint32_t v = leds_value(a);
    if (v != a->leds) {
        a->leds = v;
        a->p.panel.set_leds(a->p.panel.ctx, v);
    }
}

static void draw_track(app_t *a)
{
    screen_track_t t = { "", "", "", 0, 0, 0 };
    track_meta_t m;
    if (a->ready && a->p.source.track_meta(a->p.source.ctx, a->pl.current, &m) == 0) {
        t.title = m.title;
        t.artist = m.artist;
        t.album = m.album;
        t.duration_s = m.duration_s;
        t.track = playlist_number(&a->pl);
        t.total = a->pl.count;
    } else {
        t.title = "(esperando la lista de canciones del HPS)";
    }
    screen_draw_track(&a->p.screen, &t);
}

static void draw_message(app_t *a)
{
    char msg[80];
    uint32_t n = 0;
    if (a->src_state == SRC_FAILED)
        n = put_str(msg, n, sizeof msg, "ERROR: el HPS no encontro la SD o no hay canciones");
    else if (!a->ready)
        n = put_str(msg, n, sizeof msg, "Esperando al HPS...");
    else if (a->all_failed)
        n = put_str(msg, n, sizeof msg, "ERROR: ninguna cancion se pudo leer");
    else if (!playlist_meets_minimum(&a->pl))
        n = put_str(msg, n, sizeof msg, "Aviso: hay menos de 10 canciones en la SD");
    if (a->underruns) {
        if (n)
            n = put_str(msg, n, sizeof msg, "   ");
        n = put_str(msg, n, sizeof msg, "Subdesbordes: ");
        n = put_u32(msg, n, sizeof msg, a->underruns);
    }
    if (!n)
        msg[0] = '\0';
    screen_draw_message(&a->p.screen, msg);
}

static void draw_status(app_t *a)
{
    static const screen_state_t ST[FSM_NUM_STATES] = {
        SCREEN_STOPPED, SCREEN_PLAYING, SCREEN_PAUSED,
    };
    char filter[40];
    uint32_t n = put_str(filter, 0, sizeof filter, sw_filter_name(a->sw.filter));
    if (a->sw.invalid)
        put_str(filter, n, sizeof filter, " (SW INVALIDO)");
    screen_draw_status(&a->p.screen, ST[a->fsm.state], filter, volume_percent(&a->vol),
                       a->elapsed_s);
}

static void ui_refresh(app_t *a)
{
    leds_refresh(a);
    if (!a->dirty || a->now_ms - a->last_ui_ms < APP_UI_PERIOD_MS)
        return;
    a->last_ui_ms = a->now_ms;
    if (a->dirty & APP_DIRTY_TRACK)
        draw_track(a);
    if (a->dirty & APP_DIRTY_STATUS)
        draw_status(a);
    if (a->dirty & APP_DIRTY_MSG)
        draw_message(a);
    a->dirty = 0;
}

/* ---------- acciones de la FSM ---------- */

static void apply(app_t *a, uint32_t act)
{
    if (act & FSM_ACT_STOP) {
        a->p.source.stop(a->p.source.ctx);
        time_reset(a);
    }
    if (act & FSM_ACT_LOAD) {
        a->p.source.play(a->p.source.ctx, a->pl.current);
        a->src_state = SRC_LOADING;
        reverb_clear(a);
        time_reset(a);
    }
    if (act & FSM_ACT_TRACK) {
        a->p.panel.set_track(a->p.panel.ctx, playlist_number(&a->pl));
        a->dirty |= APP_DIRTY_TRACK;
    }
    if (act & FSM_ACT_WRAPPED)
        a->laps++;
    if (act & FSM_ACT_ALL_FAILED) {
        a->all_failed = 1;
        a->dirty |= APP_DIRTY_MSG;
    } else if (act & (FSM_ACT_LOAD | FSM_ACT_START)) {
        if (a->all_failed)
            a->dirty |= APP_DIRTY_MSG;
        a->all_failed = 0;
    }
    if (act & FSM_ACT_STATE)
        a->dirty |= APP_DIRTY_STATUS;
}

static void handle_events(app_t *a)
{
    uint8_t ev;
    while (evq_pop(a->events, &ev) == ERR_OK) {
        if (ev >= 4u || !deb_accept(&a->deb, ev, a->now_ms))
            continue;
        a->keys_accepted++;
        a->key_toggle ^= APP_LED_KEY;              /* PR-07: KEY -> LED medible */
        if (a->ready)
            apply(a, fsm_handle(&a->fsm, KEY_EVENT[ev]));
    }
}

/* ---------- fuente y audio ---------- */

static void wait_for_tracks(app_t *a)
{
    uint32_t st = a->p.source.state(a->p.source.ctx);
    if (st != a->src_state) {
        a->src_state = st;
        a->dirty |= APP_DIRTY_MSG;
    }
    if (st == SRC_WAITING || st == SRC_FAILED)
        return;
    uint32_t n = a->p.source.track_count(a->p.source.ctx);
    if (playlist_init(&a->pl, n) != ERR_OK)
        return;
    fsm_init(&a->fsm, &a->pl);
    a->ready = 1;
    a->p.panel.set_track(a->p.panel.ctx, playlist_number(&a->pl));
    a->dirty |= APP_DIRTY_TRACK | APP_DIRTY_STATUS | APP_DIRTY_MSG;
}

static void count_frame(app_t *a)
{
    a->track_frames++;
    if (++a->frames_in_sec >= APP_FS) {
        a->frames_in_sec = 0;
        a->elapsed_s++;
        a->p.panel.set_time(a->p.panel.ctx, a->elapsed_s);
        a->dirty |= APP_DIRTY_STATUS;
    }
}

static void pump_audio(app_t *a)
{
    uint32_t st = a->p.source.state(a->p.source.ctx);
    a->src_state = st;
    if (st == SRC_ERROR) {
        apply(a, fsm_handle(&a->fsm, FSM_EV_ERROR));
        return;
    }
    if (a->fsm.state != FSM_PLAYING)
        return;

    uint32_t space = a->p.audio.space(a->p.audio.ctx);
    uint32_t first_space = space, sent = 0;
    uint32_t buf[APP_CHUNK];
    int reverb = (a->sw.filter == FILTER_REVERB);
    while (space) {
        uint32_t n = a->p.source.read(a->p.source.ctx, buf, space < APP_CHUNK ? space : APP_CHUNK);
        if (!n)
            break;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t lr = buf[i];
            if (reverb) {
                int16_t l = dsp_reverb_step(&a->rev_l, (int16_t)(lr & 0xFFFFu));
                int16_t r = dsp_reverb_step(&a->rev_r, (int16_t)(lr >> 16));
                lr = (uint32_t)(uint16_t)l | ((uint32_t)(uint16_t)r << 16);
            }
            a->p.audio.write(a->p.audio.ctx, lr);
            count_frame(a);
        }
        sent += n;
        space -= n;
    }

    /* Subdesborde: la FIFO se vacio (se oye un corte) con la pista en curso. */
    if (sent) {
        a->starving = 0;
    } else if (first_space >= a->p.audio.depth && a->track_frames && st == SRC_RUNNING &&
               !a->starving) {
        a->starving = 1;
        a->underruns++;
        a->dirty |= APP_DIRTY_MSG;
    }

    if (a->p.source.state(a->p.source.ctx) == SRC_ENDED) {
        a->tracks_played++;
        apply(a, fsm_handle(&a->fsm, FSM_EV_TRACK_END));
    }
}

static void poll_inputs(app_t *a)
{
    if (a->now_ms - a->last_poll_ms < SW_POLL_MS)
        return;
    a->last_poll_ms = a->now_ms;
    filter_id_t before = a->sw.filter;
    if (sw_update(&a->sw, a->p.input.switches(a->p.input.ctx))) {
        a->p.fx.set_filter(a->p.fx.ctx, a->sw.filter);
        if (a->sw.filter == FILTER_REVERB && before != FILTER_REVERB)
            reverb_clear(a);
        a->dirty |= APP_DIRTY_STATUS;
    }
    if (volume_update(&a->vol, a->p.input.volume_adc(a->p.input.ctx))) {
        a->p.fx.set_gain(a->p.fx.ctx, a->vol.gain_q15);
        a->dirty |= APP_DIRTY_STATUS;
    }
}

/* ---------- API ---------- */

void app_init(app_t *a, const app_ports_t *ports, evq_t *events, int16_t *rev_l,
              int16_t *rev_r)
{
    uint8_t *raw = (uint8_t *)a;
    for (uint32_t i = 0; i < sizeof *a; ++i)
        raw[i] = 0;
    a->p = *ports;
    a->events = events;
    playlist_init(&a->pl, 0);
    fsm_init(&a->fsm, &a->pl);
    deb_init(&a->deb);
    dsp_reverb_init(&a->rev_l, rev_l, APP_REVERB_FRAMES);
    dsp_reverb_init(&a->rev_r, rev_r, APP_REVERB_FRAMES);
    reverb_clear(a);
    a->src_state = SRC_WAITING;

    a->now_ms = a->p.clock.now_ms(a->p.clock.ctx);
    a->last_poll_ms = a->now_ms;
    a->last_ui_ms = a->now_ms - APP_UI_PERIOD_MS;  /* primer dibujo sin esperar */

    sw_init(&a->sw, a->p.input.switches(a->p.input.ctx));
    a->p.fx.set_filter(a->p.fx.ctx, a->sw.filter);
    volume_init(&a->vol, a->p.input.volume_adc(a->p.input.ctx));
    a->p.fx.set_gain(a->p.fx.ctx, a->vol.gain_q15);

    a->p.panel.set_time(a->p.panel.ctx, 0);
    a->p.panel.set_track(a->p.panel.ctx, 0);
    a->leds = ~0u;                                 /* fuerza la primera escritura */
    screen_draw_frame(&a->p.screen);
    a->dirty = APP_DIRTY_TRACK | APP_DIRTY_STATUS | APP_DIRTY_MSG;
    ui_refresh(a);
}

void app_step(app_t *a)
{
    a->now_ms = a->p.clock.now_ms(a->p.clock.ctx);
    if (!a->ready)
        wait_for_tracks(a);
    handle_events(a);
    if (a->ready)
        pump_audio(a);
    poll_inputs(a);
    ui_refresh(a);
}
