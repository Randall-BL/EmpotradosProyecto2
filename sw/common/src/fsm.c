/*
 * Maquina de estados de reproduccion (SC-08). Ver fsm.h para la tabla en palabras.
 *
 * La logica esta en una tabla constante table[estado][evento] con tres datos por
 * celda: estado siguiente, movimiento en la lista y acciones para el lazo principal.
 * fsm_handle() solo busca la celda, mueve la playlist, cuenta errores seguidos y
 * devuelve las acciones; no toca hardware.
 */
#include "fsm.h"

/* Movimiento que una transicion hace en la playlist. */
typedef enum {
    NAV_NONE = 0,                          /* no cambia la pista */
    NAV_NEXT,                              /* siguiente (KEY3), con vuelta al inicio */
    NAV_PREV,                              /* anterior (KEY2), con vuelta al final */
    NAV_AUTO,                              /* avance automatico al terminar la pista */
    NAV_SKIP                               /* salto por error de lectura */
} nav_t;

/* Una celda de la tabla. Se guarda en bytes para que la tabla ocupe poco. */
typedef struct {
    uint8_t next;                          /* fsm_state_t */
    uint8_t nav;                           /* nav_t */
    uint8_t act;                           /* FSM_ACT_* (los 6 bits bajos) */
} transition_t;

/* Abreviaturas solo para que la tabla quepa en una linea por celda. */
#define S FSM_STOPPED
#define P FSM_PLAYING
#define Z FSM_PAUSED
#define LOAD  FSM_ACT_LOAD
#define START FSM_ACT_START
#define PAUSE FSM_ACT_PAUSE
#define STOP  FSM_ACT_STOP
#define TRACK FSM_ACT_TRACK

/* Tabla de transiciones: { estado siguiente, movimiento, acciones }. */
static const transition_t table[FSM_NUM_STATES][FSM_NUM_EVENTS] = {
    /* DETENIDO: PLAY carga y arranca la pista seleccionada; PREV/NEXT solo cambian la
     * seleccion; fin de pista y error no aplican (no hay flujo). */
    [FSM_STOPPED] = {
        [FSM_EV_PLAY_PAUSE] = { P, NAV_NONE, LOAD | START },
        [FSM_EV_STOP]       = { S, NAV_NONE, 0 },
        [FSM_EV_PREV]       = { S, NAV_PREV, TRACK },
        [FSM_EV_NEXT]       = { S, NAV_NEXT, TRACK },
        [FSM_EV_TRACK_END]  = { S, NAV_NONE, 0 },
        [FSM_EV_ERROR]      = { S, NAV_NONE, 0 },
    },
    /* REPRODUCIENDO: PLAY pausa; STOP detiene; PREV/NEXT/fin/error cambian de pista
     * y la reproducen desde el inicio. */
    [FSM_PLAYING] = {
        [FSM_EV_PLAY_PAUSE] = { Z, NAV_NONE, PAUSE },
        [FSM_EV_STOP]       = { S, NAV_NONE, STOP },
        [FSM_EV_PREV]       = { P, NAV_PREV, TRACK | LOAD | START },
        [FSM_EV_NEXT]       = { P, NAV_NEXT, TRACK | LOAD | START },
        [FSM_EV_TRACK_END]  = { P, NAV_AUTO, TRACK | LOAD | START },
        [FSM_EV_ERROR]      = { P, NAV_SKIP, TRACK | LOAD | START },
    },
    /* PAUSADO: PLAY reanuda donde quedo; PREV/NEXT cargan otra pista pero siguen en
     * pausa; error salta a la siguiente sin arrancarla. */
    [FSM_PAUSED] = {
        [FSM_EV_PLAY_PAUSE] = { P, NAV_NONE, START },
        [FSM_EV_STOP]       = { S, NAV_NONE, STOP },
        [FSM_EV_PREV]       = { Z, NAV_PREV, TRACK | LOAD },
        [FSM_EV_NEXT]       = { Z, NAV_NEXT, TRACK | LOAD },
        [FSM_EV_TRACK_END]  = { Z, NAV_NONE, 0 },
        [FSM_EV_ERROR]      = { Z, NAV_SKIP, TRACK | LOAD },
    },
};

/* Arranca en DETENIDO sobre la playlist pl (que debe vivir mientras se use la FSM). */
void fsm_init(fsm_t *f, playlist_t *pl)
{
    f->state = FSM_STOPPED;
    f->pl = pl;
    f->errors = 0;
}

/*
 * Procesa el evento ev: busca la transicion, mueve la playlist y cambia de estado.
 * Devuelve las acciones FSM_ACT_* que el lazo principal debe ejecutar (0 si el
 * evento no es valido o la lista esta vacia).
 */
uint32_t fsm_handle(fsm_t *f, fsm_event_t ev)
{
    if ((unsigned)ev >= FSM_NUM_EVENTS || (unsigned)f->state >= FSM_NUM_STATES ||
        !f->pl || !f->pl->count)
        return 0;
    const transition_t *t = &table[f->state][ev];
    uint32_t act = t->act;

    /* Errores seguidos: si fallan tantas pistas como hay en la lista, ninguna se puede
     * leer; se detiene en vez de saltar sin fin. Cualquier otro evento (fin de pista o
     * accion del usuario) reinicia la cuenta. */
    if (ev == FSM_EV_ERROR && t->nav == NAV_SKIP) {
        if (++f->errors >= f->pl->count) {
            f->errors = 0;
            f->state = FSM_STOPPED;
            return FSM_ACT_STOP | FSM_ACT_STATE | FSM_ACT_ALL_FAILED;
        }
    } else if (ev != FSM_EV_ERROR) {
        f->errors = 0;
    }

    /* Movimiento en la lista. */
    switch ((nav_t)t->nav) {
    case NAV_NEXT:
    case NAV_SKIP:
        playlist_next(f->pl);
        break;
    case NAV_PREV:
        playlist_prev(f->pl);
        break;
    case NAV_AUTO:
        /* Al terminar la ultima pista se vuelve a la primera (la lista se repite). */
        if (playlist_auto_advance(f->pl))
            act |= FSM_ACT_WRAPPED;
        break;
    case NAV_NONE:
        break;
    }
    /* Cambio de estado: se avisa para redibujar estado y LEDs. */
    if (t->next != f->state) {
        f->state = (fsm_state_t)t->next;
        act |= FSM_ACT_STATE;
    }
    return act;
}

/* Nombre del estado para mostrar en la consola o en las pruebas. */
const char *fsm_state_name(fsm_state_t s)
{
    switch (s) {
    case FSM_STOPPED: return "DETENIDO";
    case FSM_PLAYING: return "REPRODUCIENDO";
    case FSM_PAUSED:  return "PAUSADO";
    default:          return "?";
    }
}
