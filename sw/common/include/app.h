/*
 * Lazo principal del reproductor (NV-14), portable.
 *
 * app_step() es una vuelta del lazo: no bloquea y no toca hardware (todo pasa por los
 * puertos de ports.h). El main del Nios V lo llama sin fin; las pruebas del PC lo
 * llaman con mocks y un reloj simulado.
 *
 * En cada vuelta:
 *   1. Eventos de botones (cola llenada por la ISR, SC-09) -> antirrebote (NV-04) ->
 *      FSM (SC-08) -> acciones sobre la fuente y la pantalla.
 *   2. Bombeo de audio sin bloquear: mientras la FIFO del codec tenga espacio, toma
 *      cuadros del ring del HPS, aplica la reverberacion si esta elegida y los
 *      escribe (el filtro de hardware y el volumen van dentro del puerto de audio).
 *      Cuenta los subdesbordes: la FIFO quedo vacia con la pista en curso.
 *   3. Cada 20 ms: switches -> filtro (CD-08) y potenciometro -> volumen (CD-05).
 *   4. LEDs y 7 segmentos al cambiar; pantalla limitada a 10 Hz.
 *
 * Tiempo transcurrido (CD-10): cuadros enviados al codec / 48 000; se detiene solo en
 * pausa y se reinicia al cargar otra pista o con stop.
 *
 * LEDR (CD-08):
 *   [2:0] filtro activo (one-hot: 0 pasa-bajos, 1 pasa-banda, 2 reverberacion)
 *   [5]   hubo al menos un subdesborde      [6] cambia con cada boton aceptado (PR-07)
 *   [7]   reproduciendo                     [8] pausado
 *   [9]   alerta: switches invalidos, fuente sin SD o ninguna pista se pudo leer
 */
#ifndef COMMON_APP_H
#define COMMON_APP_H

#include <stdint.h>
#include "debounce.h"
#include "dsp.h"
#include "evq.h"
#include "fsm.h"
#include "playlist.h"
#include "ports.h"
#include "switches.h"
#include "volume.h"

#define APP_FS            48000u           /* el ring siempre llega a 48 kHz (CD-07) */
#define APP_UI_PERIOD_MS  100u             /* pantalla a 10 Hz */
#define APP_CHUNK         32u              /* cuadros por lectura del ring */
#define APP_REVERB_FRAMES 2400u            /* 50 ms a 48 kHz */

/* Eventos de la cola: 0..3 = KEY0..KEY3 presionado. */
#define APP_EV_KEY(n)     ((uint8_t)(n))

#define APP_LED_FILTER    0x007u
#define APP_LED_UNDERRUN  (1u << 5)
#define APP_LED_KEY       (1u << 6)
#define APP_LED_PLAYING   (1u << 7)
#define APP_LED_PAUSED    (1u << 8)
#define APP_LED_ALERT     (1u << 9)

#define APP_DIRTY_TRACK   (1u << 0)
#define APP_DIRTY_STATUS  (1u << 1)
#define APP_DIRTY_MSG     (1u << 2)

typedef struct {
    app_ports_t p;
    evq_t *events;

    playlist_t pl;
    fsm_t fsm;
    deb_t deb;
    sw_state_t sw;
    volume_t vol;
    dsp_reverb_t rev_l, rev_r;

    int ready;                             /* hay tabla de pistas */
    int all_failed;                        /* ninguna pista se pudo leer */
    uint32_t src_state;                    /* ultimo SRC_* visto */
    uint32_t now_ms;
    uint32_t last_poll_ms, last_ui_ms;
    uint32_t dirty;
    uint32_t leds;                         /* ultimo valor escrito en LEDR */
    uint32_t key_toggle;

    uint32_t frames_in_sec;                /* cuadros enviados en el segundo actual */
    uint32_t elapsed_s;
    uint32_t track_frames;                 /* cuadros enviados de la pista actual */
    uint32_t underruns;
    int starving;                          /* subdesborde en curso (se cuenta una vez) */
    uint32_t laps;                         /* vueltas completas a la lista (PR-08) */
    uint32_t tracks_played;                /* pistas terminadas por fin de archivo */
    uint32_t keys_accepted;
} app_t;

/* Prepara el estado. rev_l/rev_r: buffers de APP_REVERB_FRAMES muestras (sin malloc).
 * Dibuja el marco de la pantalla y deja todo en DETENIDO esperando la tabla. */
void app_init(app_t *a, const app_ports_t *ports, evq_t *events, int16_t *rev_l,
              int16_t *rev_r);

/* Una vuelta del lazo principal. */
void app_step(app_t *a);

/* Para las pruebas y las mediciones. */
static inline fsm_state_t app_state(const app_t *a) { return a->fsm.state; }

#endif /* COMMON_APP_H */
