/*
 * Maquina de estados de reproduccion (SC-08, decision CD-11: en software, en
 * sw/common, probada en el PC).
 *
 * Es una tabla de transiciones [estado][evento] -> (estado siguiente, acciones,
 * movimiento en la lista). fsm_handle() no toca hardware: aplica el movimiento a la
 * playlist y devuelve una mascara de acciones FSM_ACT_* que el lazo principal (app.c)
 * ejecuta con los puertos (SC-14).
 *
 *   evento \ estado   DETENIDO              REPRODUCIENDO             PAUSADO
 *   PLAY_PAUSE        -> REPR. (carga)      -> PAUSADO                -> REPR. (reanuda)
 *   STOP              (nada)                -> DETENIDO (al inicio)   -> DETENIDO
 *   NEXT / PREV       cambia la seleccion   cambia y reproduce        cambia, sigue en pausa
 *   TRACK_END         (nada)                avanza y reproduce (vuelve al inicio al final)
 *   ERROR             (nada)                salta a la siguiente      salta a la siguiente
 *
 * Si fallan todas las pistas seguidas (ERROR count veces sin un TRACK_END ni accion
 * del usuario en medio) se detiene con FSM_ACT_ALL_FAILED para no girar sin fin.
 */
#ifndef COMMON_FSM_H
#define COMMON_FSM_H

#include <stdint.h>
#include "playlist.h"

typedef enum {
    FSM_STOPPED = 0,
    FSM_PLAYING,
    FSM_PAUSED,
    FSM_NUM_STATES
} fsm_state_t;

typedef enum {
    FSM_EV_PLAY_PAUSE = 0,                 /* KEY0 */
    FSM_EV_STOP,                           /* KEY1 */
    FSM_EV_PREV,                           /* KEY2 */
    FSM_EV_NEXT,                           /* KEY3 */
    FSM_EV_TRACK_END,                      /* el HPS termino de entregar la pista */
    FSM_EV_ERROR,                          /* la pista no se pudo leer */
    FSM_NUM_EVENTS
} fsm_event_t;

/* Acciones para el lazo principal. */
#define FSM_ACT_LOAD       (1u << 0)   /* pedir la pista actual al HPS, tiempo a 0 */
#define FSM_ACT_START      (1u << 1)   /* consumir y enviar audio al codec */
#define FSM_ACT_PAUSE      (1u << 2)   /* dejar de consumir (se conserva la posicion) */
#define FSM_ACT_STOP       (1u << 3)   /* parar el flujo y volver al inicio de la pista */
#define FSM_ACT_TRACK      (1u << 4)   /* cambio la pista actual: redibujar sus datos */
#define FSM_ACT_STATE      (1u << 5)   /* cambio el estado: redibujar estado y LEDs */
#define FSM_ACT_WRAPPED    (1u << 6)   /* el avance automatico dio la vuelta a la lista */
#define FSM_ACT_ALL_FAILED (1u << 7)   /* ninguna pista se pudo reproducir */

typedef struct {
    fsm_state_t state;
    playlist_t *pl;
    uint32_t errors;                       /* errores seguidos */
} fsm_t;

void fsm_init(fsm_t *f, playlist_t *pl);

/* Procesa un evento: actualiza estado y playlist, devuelve FSM_ACT_*. Un evento fuera
 * de rango o con la lista vacia no hace nada (devuelve 0). */
uint32_t fsm_handle(fsm_t *f, fsm_event_t ev);

const char *fsm_state_name(fsm_state_t s);

#endif /* COMMON_FSM_H */
