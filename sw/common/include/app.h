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

/* Eventos de la cola: 0..3 = KEY0..KEY3 presionado. La ISR de botones empuja
 * APP_EV_KEY(n) en la cola y app_step() lo traduce a un evento de la FSM. */
#define APP_EV_KEY(n)     ((uint8_t)(n))

/* Bits de LEDR que maneja el lazo principal (ver la tabla de arriba). */
#define APP_LED_FILTER    0x007u           /* LEDR[2:0]: filtro activo (one-hot) */
#define APP_LED_UNDERRUN  (1u << 5)        /* LEDR5: hubo al menos un subdesborde */
#define APP_LED_KEY       (1u << 6)        /* LEDR6: conmuta con cada boton aceptado */
#define APP_LED_PLAYING   (1u << 7)        /* LEDR7: estado REPRODUCIENDO */
#define APP_LED_PAUSED    (1u << 8)        /* LEDR8: estado PAUSADO */
#define APP_LED_ALERT     (1u << 9)        /* LEDR9: alguna condicion de error */

/* Marcas de "zona sucia" de la pantalla: indican que parte hay que redibujar en la
 * siguiente actualizacion (limitada a 10 Hz). Redibujar solo lo que cambio ahorra
 * escrituras lentas al Character Buffer. */
#define APP_DIRTY_TRACK   (1u << 0)        /* titulo, artista, album, duracion, n/total */
#define APP_DIRTY_STATUS  (1u << 1)        /* estado, filtro, volumen y tiempo */
#define APP_DIRTY_MSG     (1u << 2)        /* linea de mensajes (avisos y errores) */

/* Estado completo del reproductor. No usa memoria dinamica: el main lo declara como
 * variable estatica y le pasa los buffers de la reverberacion. */
typedef struct {
    app_ports_t p;                         /* puertos al hardware (copia de los de app_init) */
    evq_t *events;                         /* cola de botones que llena la ISR */

    playlist_t pl;                         /* lista de pistas: cantidad y pista actual */
    fsm_t fsm;                             /* maquina de estados DETENIDO/REPR./PAUSADO */
    deb_t deb;                             /* antirrebote por ventana de tiempo */
    sw_state_t sw;                         /* filtro elegido con los switches */
    volume_t vol;                          /* escalon de volumen del potenciometro */
    dsp_reverb_t rev_l, rev_r;             /* reverberacion por software, un canal cada una */

    int ready;                             /* hay tabla de pistas */
    int all_failed;                        /* ninguna pista se pudo leer */
    uint32_t src_state;                    /* ultimo SRC_* visto */
    uint32_t now_ms;                       /* reloj leido al inicio de la vuelta actual */
    uint32_t last_poll_ms, last_ui_ms;     /* ultima lectura de entradas / ultimo dibujo */
    uint32_t dirty;                        /* APP_DIRTY_*: partes de la pantalla a redibujar */
    uint32_t leds;                         /* ultimo valor escrito en LEDR */
    uint32_t key_toggle;                   /* 0 o APP_LED_KEY: conmuta con cada boton */

    uint32_t frames_in_sec;                /* cuadros enviados en el segundo actual */
    uint32_t elapsed_s;                    /* segundos transcurridos de la pista (CD-10) */
    uint32_t track_frames;                 /* cuadros enviados de la pista actual */
    uint32_t underruns;                    /* subdesbordes contados desde el arranque */
    int starving;                          /* subdesborde en curso (se cuenta una vez) */
    uint32_t laps;                         /* vueltas completas a la lista (PR-08) */
    uint32_t tracks_played;                /* pistas terminadas por fin de archivo */
    uint32_t keys_accepted;                /* botones que pasaron el antirrebote */
} app_t;

/* Prepara el estado. rev_l/rev_r: buffers de APP_REVERB_FRAMES muestras (sin malloc).
 * Dibuja el marco de la pantalla y deja todo en DETENIDO esperando la tabla. */
void app_init(app_t *a, const app_ports_t *ports, evq_t *events, int16_t *rev_l,
              int16_t *rev_r);

/* Una vuelta del lazo principal: lee el reloj, espera la tabla de pistas si todavia
 * no llego, atiende los botones, bombea audio al codec, lee switches y potenciometro y
 * refresca LEDs y pantalla. No bloquea: se llama sin fin desde el main. */
void app_step(app_t *a);

/* Estado actual de la FSM (DETENIDO, REPRODUCIENDO o PAUSADO). Para las pruebas y
 * las mediciones. */
static inline fsm_state_t app_state(const app_t *a) { return a->fsm.state; }

#endif /* COMMON_APP_H */
