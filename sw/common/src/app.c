/*
 * Lazo principal del reproductor (NV-14). Ver app.h para el resumen de cada vuelta y
 * el significado de los LEDs.
 *
 * Organizacion del archivo:
 *   - utilidades de texto sin printf (put_str, put_u32);
 *   - reinicio de la reverberacion y del tiempo transcurrido;
 *   - LEDs y pantalla (solo se redibuja lo marcado como "sucio", a 10 Hz como mucho);
 *   - aplicacion de las acciones que devuelve la FSM;
 *   - espera de la tabla de pistas, bombeo de audio y lectura de entradas;
 *   - API publica: app_init() y app_step().
 *
 * Nada de este archivo toca registros: todo pasa por los puertos de ports.h, asi el
 * mismo codigo corre en el Nios V y en las pruebas del PC (tests/test_app.c).
 */
#include "app.h"

/* KEY0..KEY3 -> evento de la FSM (la ayuda de la pantalla dice lo mismo). El indice es
 * el numero de boton que la ISR mete en la cola. */
static const fsm_event_t KEY_EVENT[4] = {
    FSM_EV_PLAY_PAUSE, FSM_EV_STOP, FSM_EV_PREV, FSM_EV_NEXT,
};

/* ---------- utilidades sin printf (el Nios V/m no tiene divisor) ---------- */

/*
 * Copia la cadena s en dst a partir de la posicion pos, sin pasarse de max bytes
 * (deja siempre lugar para el '\0' final). Devuelve la nueva posicion de escritura,
 * para poder encadenar llamadas: n = put_str(buf, n, ...); n = put_u32(buf, n, ...).
 */
static uint32_t put_str(char *dst, uint32_t pos, uint32_t max, const char *s)
{
    while (*s && pos + 1u < max)
        dst[pos++] = *s++;
    dst[pos] = '\0';
    return pos;
}

/*
 * Escribe v en decimal en dst a partir de pos, sin ceros a la izquierda. No usa
 * division: para cada potencia de 10 (de 10^9 a 10^0) resta esa potencia mientras
 * quepa y cuenta cuantas veces lo hizo; ese conteo es el digito. Devuelve la nueva
 * posicion de escritura (igual que put_str).
 */
static uint32_t put_u32(char *dst, uint32_t pos, uint32_t max, uint32_t v)
{
    static const uint32_t pow10[] = { 1000000000u, 100000000u, 10000000u, 1000000u,
                                      100000u, 10000u, 1000u, 100u, 10u, 1u };
    int started = 0;                       /* 1 al aparecer el primer digito no cero */
    for (uint32_t i = 0; i < 10u; ++i) {
        /* Digito de la posicion i: cuantas veces cabe pow10[i] en lo que queda. */
        char d = '0';
        while (v >= pow10[i]) {
            v -= pow10[i];
            d++;
        }
        /* Se omiten los ceros a la izquierda, salvo el de las unidades (v == 0 -> "0"). */
        if (d != '0' || started || i == 9u) {
            started = 1;
            if (pos + 1u < max)
                dst[pos++] = d;
        }
    }
    dst[pos] = '\0';
    return pos;
}

/*
 * Borra las lineas de retardo de la reverberacion (los dos canales) y vuelve su
 * indice al inicio. Se llama al cargar otra pista o al activar la reverberacion para
 * que no se oiga el "eco" de audio viejo.
 */
static void reverb_clear(app_t *a)
{
    for (uint32_t i = 0; i < APP_REVERB_FRAMES; ++i) {
        a->rev_l.buf[i] = 0;
        a->rev_r.buf[i] = 0;
    }
    a->rev_l.pos = a->rev_r.pos = 0;
}

/*
 * Vuelve a cero el tiempo transcurrido de la pista (CD-10): contadores de cuadros,
 * segundos y la marca de subdesborde en curso. Actualiza el 7 segmentos de inmediato
 * y marca la linea de estado para redibujarla.
 */
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

/*
 * Calcula el valor que deberia tener LEDR segun el estado actual (ver la tabla de
 * app.h): filtro activo en [2:0], conmutador de botones en [6], subdesborde en [5],
 * reproduciendo/pausado en [7]/[8] y alerta en [9].
 */
static uint32_t leds_value(const app_t *a)
{
    /* filter_id_t -> patron one-hot de LEDR[2:0]. Bypass no enciende ninguno. */
    static const uint32_t FILTER_LED[FILTER_COUNT] = { 0u, 1u, 2u, 4u };
    uint32_t v = FILTER_LED[a->sw.filter] | a->key_toggle;
    if (a->underruns)
        v |= APP_LED_UNDERRUN;
    if (a->fsm.state == FSM_PLAYING)
        v |= APP_LED_PLAYING;
    else if (a->fsm.state == FSM_PAUSED)
        v |= APP_LED_PAUSED;
    /* Alerta: switches en combinacion invalida, todas las pistas fallaron o el HPS
     * reporto que no puede funcionar (sin SD o sin canciones). */
    if (a->sw.invalid || a->all_failed || a->src_state == SRC_FAILED)
        v |= APP_LED_ALERT;
    return v;
}

/*
 * Escribe LEDR solo si el valor cambio desde la ultima vez. Los LEDs no tienen limite
 * de frecuencia (a diferencia de la pantalla) porque escribirlos es un solo store;
 * asi el LED de botones responde en microsegundos (PR-07).
 */
static void leds_refresh(app_t *a)
{
    uint32_t v = leds_value(a);
    if (v != a->leds) {
        a->leds = v;
        a->p.panel.set_leds(a->p.panel.ctx, v);
    }
}

/*
 * Dibuja el bloque de la cancion actual: titulo, artista, album, duracion y "n/total".
 * Los textos los entrega la fuente (copiados de la tabla que publico el HPS). Si
 * todavia no hay tabla, o la fuente no puede dar los datos, se muestra un aviso de
 * espera en el lugar del titulo.
 */
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

/*
 * Arma y dibuja la linea de mensajes (fila 7). Se muestra el aviso mas importante
 * (en orden: HPS sin SD, esperando al HPS, todas fallaron, menos de 10 canciones) y,
 * si hubo subdesbordes, su cantidad a continuacion. Sin nada que avisar la linea se
 * borra (cadena vacia).
 */
static void draw_message(app_t *a)
{
    char msg[80];
    uint32_t n = 0;                        /* largo del mensaje armado hasta ahora */
    if (a->src_state == SRC_FAILED)
        n = put_str(msg, n, sizeof msg, "ERROR: el HPS no encontro la SD o no hay canciones");
    else if (!a->ready)
        n = put_str(msg, n, sizeof msg, "Esperando al HPS...");
    else if (a->all_failed)
        n = put_str(msg, n, sizeof msg, "ERROR: ninguna cancion se pudo leer");
    else if (!playlist_meets_minimum(&a->pl))
        n = put_str(msg, n, sizeof msg, "Aviso: hay menos de 10 canciones en la SD");
    if (a->underruns) {
        /* Separador solo si ya habia un aviso antes. */
        if (n)
            n = put_str(msg, n, sizeof msg, "   ");
        n = put_str(msg, n, sizeof msg, "Subdesbordes: ");
        n = put_u32(msg, n, sizeof msg, a->underruns);
    }
    if (!n)
        msg[0] = '\0';
    screen_draw_message(&a->p.screen, msg);
}

/*
 * Dibuja la zona de estado: estado de la FSM, nombre del filtro (con la marca
 * "SW INVALIDO" si hay mas de un switch arriba), porcentaje de volumen y tiempo
 * transcurrido.
 */
static void draw_status(app_t *a)
{
    /* fsm_state_t -> screen_state_t (mismo orden; se traduce para no acoplar screen.h
     * con fsm.h). */
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

/*
 * Refresca los indicadores. Los LEDs se actualizan en cada vuelta; la pantalla solo
 * si hay algo marcado como sucio y pasaron al menos APP_UI_PERIOD_MS (100 ms) desde el
 * ultimo dibujo. Escribir el Character Buffer es lento comparado con el bombeo de
 * audio, por eso se limita a 10 Hz y se redibuja solo la zona que cambio.
 */
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

/*
 * Ejecuta la mascara de acciones FSM_ACT_* que devolvio fsm_handle(). La FSM solo
 * decide; aqui se hacen los efectos con los puertos:
 *   STOP       -> la fuente deja de entregar audio y el tiempo vuelve a 0.
 *   LOAD       -> se pide al HPS la pista actual desde el inicio; se limpia la
 *                 reverberacion y el tiempo.
 *   TRACK      -> cambio la pista seleccionada: 7 segmentos y datos en pantalla.
 *   WRAPPED    -> el avance automatico dio la vuelta a la lista (se cuenta, PR-08).
 *   ALL_FAILED -> ninguna pista se pudo leer: se marca la alerta.
 *   STATE      -> cambio el estado: redibujar la zona de estado.
 * START y PAUSE no necesitan efecto propio: pump_audio() solo envia audio mientras
 * la FSM esta en REPRODUCIENDO.
 */
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
        /* El usuario volvio a intentar reproducir: se quita la alerta anterior. */
        if (a->all_failed)
            a->dirty |= APP_DIRTY_MSG;
        a->all_failed = 0;
    }
    if (act & FSM_ACT_STATE)
        a->dirty |= APP_DIRTY_STATUS;
}

/*
 * Vacia la cola de botones que llena la ISR. Cada evento pasa por el antirrebote; los
 * aceptados conmutan LEDR6 (para medir la latencia boton -> LED, PR-07) y, si ya hay
 * tabla de pistas, se traducen a un evento de la FSM y se aplican sus acciones. Antes
 * de tener la tabla los botones no hacen nada (no hay que reproducir).
 */
static void handle_events(app_t *a)
{
    uint8_t ev;
    while (evq_pop(a->events, &ev) == ERR_OK) {
        /* Codigos fuera de 0..3 o rebotes dentro de la ventana se descartan. */
        if (ev >= 4u || !deb_accept(&a->deb, ev, a->now_ms))
            continue;
        a->keys_accepted++;
        a->key_toggle ^= APP_LED_KEY;              /* PR-07: KEY -> LED medible */
        if (a->ready)
            apply(a, fsm_handle(&a->fsm, KEY_EVENT[ev]));
    }
}

/* ---------- fuente y audio ---------- */

/*
 * Mientras no hay tabla de pistas: consulta el estado de la fuente (el HPS puede estar
 * arrancando, o reportar que no hay SD). Cuando la tabla esta lista, crea la playlist
 * con la cantidad de pistas, arranca la FSM en DETENIDO y marca toda la pantalla para
 * redibujar. Si la tabla viene vacia sigue esperando.
 */
static void wait_for_tracks(app_t *a)
{
    uint32_t st = a->p.source.state(a->p.source.ctx);
    if (st != a->src_state) {
        /* Cambio el estado de la fuente: puede cambiar el mensaje (espera / error). */
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

/*
 * Cuenta un cuadro enviado al codec. Cada APP_FS (48 000) cuadros pasa un segundo de
 * audio: se incrementa el tiempo transcurrido y se actualiza el 7 segmentos. Contar
 * cuadros (en vez de usar el reloj) hace que el tiempo se detenga solo en pausa y que
 * coincida exactamente con lo que se oye (CD-10).
 */
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

/*
 * Bombeo de audio sin bloquear. Pasos:
 *   1. Si la fuente reporta error (la pista no se pudo leer), se avisa a la FSM, que
 *      salta a la siguiente pista.
 *   2. Si no se esta reproduciendo no se consume nada (pausa = el ring se queda como
 *      esta y la posicion se conserva).
 *   3. Mientras la FIFO del codec tenga espacio, se leen bloques de hasta APP_CHUNK
 *      cuadros del ring, se les aplica la reverberacion si es el filtro elegido y se
 *      escriben en la FIFO (el filtro FIR y el volumen los aplica el hardware).
 *   4. Deteccion de subdesborde: la FIFO estaba completamente vacia y no habia nada
 *      para mandar, con la pista en curso. Se cuenta una sola vez por episodio.
 *   5. Si la fuente termino la pista (EOF y ring vacio), se avisa a la FSM para que
 *      avance a la siguiente.
 */
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

    uint32_t space = a->p.audio.space(a->p.audio.ctx);   /* cuadros libres en la FIFO */
    uint32_t first_space = space, sent = 0;
    uint32_t buf[APP_CHUNK];                               /* bloque leido del ring */
    int reverb = (a->sw.filter == FILTER_REVERB);
    while (space) {
        /* Se pide como mucho lo que cabe en la FIFO y en el buffer local. */
        uint32_t n = a->p.source.read(a->p.source.ctx, buf, space < APP_CHUNK ? space : APP_CHUNK);
        if (!n)
            break;                                         /* el ring esta vacio */
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t lr = buf[i];                          /* L en [15:0], R en [31:16] */
            if (reverb) {
                /* Se separan los canales, se procesa cada uno con su propia linea de
                 * retardo y se vuelven a empaquetar. */
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

    /* Subdesborde: la FIFO se vacio (se oye un corte) con la pista en curso. Solo
     * cuenta si ya se habia enviado audio de esta pista (el arranque no es corte) y la
     * fuente sigue entregando (no es el final de la pista). */
    if (sent) {
        a->starving = 0;
    } else if (first_space >= a->p.audio.depth && a->track_frames && st == SRC_RUNNING &&
               !a->starving) {
        a->starving = 1;
        a->underruns++;
        a->dirty |= APP_DIRTY_MSG;
    }

    /* Se vuelve a consultar el estado: la lectura de arriba pudo vaciar el ring y dejar
     * la pista como terminada. */
    if (a->p.source.state(a->p.source.ctx) == SRC_ENDED) {
        a->tracks_played++;
        apply(a, fsm_handle(&a->fsm, FSM_EV_TRACK_END));
    }
}

/*
 * Lectura periodica (cada SW_POLL_MS = 20 ms) de las entradas que no generan
 * interrupcion:
 *   - switches: si cambia el filtro se programa el modulo propio (la reverberacion
 *     deja el FIR en bypass) y, al entrar a reverberacion, se limpia su linea de
 *     retardo;
 *   - potenciometro: si cambia el escalon de volumen se escribe la ganancia nueva en
 *     el hardware.
 */
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

/*
 * Deja el reproductor listo para arrancar:
 *   - pone toda la estructura en cero (sin memset para no depender de la libc);
 *   - copia los puertos y prepara playlist vacia, FSM, antirrebote y reverberacion;
 *   - lee los switches y el potenciometro para arrancar con el filtro y el volumen que
 *     ya estan puestos, y los programa en el hardware;
 *   - apaga los indicadores, dibuja el marco de la pantalla y fuerza el primer dibujo
 *     completo.
 * La fuente queda en SRC_WAITING: app_step() esperara la tabla de pistas del HPS.
 */
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

    /* Filtro y volumen iniciales segun la posicion actual de los controles. */
    sw_init(&a->sw, a->p.input.switches(a->p.input.ctx));
    a->p.fx.set_filter(a->p.fx.ctx, a->sw.filter);
    volume_init(&a->vol, a->p.input.volume_adc(a->p.input.ctx));
    a->p.fx.set_gain(a->p.fx.ctx, a->vol.gain_q15);

    /* Indicadores en su estado inicial y primer dibujo de la pantalla. */
    a->p.panel.set_time(a->p.panel.ctx, 0);
    a->p.panel.set_track(a->p.panel.ctx, 0);
    a->leds = ~0u;                                 /* fuerza la primera escritura */
    screen_draw_frame(&a->p.screen);
    a->dirty = APP_DIRTY_TRACK | APP_DIRTY_STATUS | APP_DIRTY_MSG;
    ui_refresh(a);
}

/*
 * Una vuelta del lazo principal (ver el orden en app.h). Se toma el reloj una vez al
 * inicio para que todas las decisiones de la vuelta usen el mismo instante.
 */
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
