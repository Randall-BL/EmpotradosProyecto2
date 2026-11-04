#include "fsm.h"

typedef enum { NAV_NONE = 0, NAV_NEXT, NAV_PREV, NAV_AUTO, NAV_SKIP } nav_t;

typedef struct {
    uint8_t next;                          /* fsm_state_t */
    uint8_t nav;                           /* nav_t */
    uint8_t act;                           /* FSM_ACT_* (los 6 bits bajos) */
} transition_t;

#define S FSM_STOPPED
#define P FSM_PLAYING
#define Z FSM_PAUSED
#define LOAD  FSM_ACT_LOAD
#define START FSM_ACT_START
#define PAUSE FSM_ACT_PAUSE
#define STOP  FSM_ACT_STOP
#define TRACK FSM_ACT_TRACK

static const transition_t table[FSM_NUM_STATES][FSM_NUM_EVENTS] = {
    [FSM_STOPPED] = {
        [FSM_EV_PLAY_PAUSE] = { P, NAV_NONE, LOAD | START },
        [FSM_EV_STOP]       = { S, NAV_NONE, 0 },
        [FSM_EV_PREV]       = { S, NAV_PREV, TRACK },
        [FSM_EV_NEXT]       = { S, NAV_NEXT, TRACK },
        [FSM_EV_TRACK_END]  = { S, NAV_NONE, 0 },
        [FSM_EV_ERROR]      = { S, NAV_NONE, 0 },
    },
    [FSM_PLAYING] = {
        [FSM_EV_PLAY_PAUSE] = { Z, NAV_NONE, PAUSE },
        [FSM_EV_STOP]       = { S, NAV_NONE, STOP },
        [FSM_EV_PREV]       = { P, NAV_PREV, TRACK | LOAD | START },
        [FSM_EV_NEXT]       = { P, NAV_NEXT, TRACK | LOAD | START },
        [FSM_EV_TRACK_END]  = { P, NAV_AUTO, TRACK | LOAD | START },
        [FSM_EV_ERROR]      = { P, NAV_SKIP, TRACK | LOAD | START },
    },
    [FSM_PAUSED] = {
        [FSM_EV_PLAY_PAUSE] = { P, NAV_NONE, START },
        [FSM_EV_STOP]       = { S, NAV_NONE, STOP },
        [FSM_EV_PREV]       = { Z, NAV_PREV, TRACK | LOAD },
        [FSM_EV_NEXT]       = { Z, NAV_NEXT, TRACK | LOAD },
        [FSM_EV_TRACK_END]  = { Z, NAV_NONE, 0 },
        [FSM_EV_ERROR]      = { Z, NAV_SKIP, TRACK | LOAD },
    },
};

void fsm_init(fsm_t *f, playlist_t *pl)
{
    f->state = FSM_STOPPED;
    f->pl = pl;
    f->errors = 0;
}

uint32_t fsm_handle(fsm_t *f, fsm_event_t ev)
{
    if ((unsigned)ev >= FSM_NUM_EVENTS || (unsigned)f->state >= FSM_NUM_STATES ||
        !f->pl || !f->pl->count)
        return 0;
    const transition_t *t = &table[f->state][ev];
    uint32_t act = t->act;

    if (ev == FSM_EV_ERROR && t->nav == NAV_SKIP) {
        if (++f->errors >= f->pl->count) {
            f->errors = 0;
            f->state = FSM_STOPPED;
            return FSM_ACT_STOP | FSM_ACT_STATE | FSM_ACT_ALL_FAILED;
        }
    } else if (ev != FSM_EV_ERROR) {
        f->errors = 0;
    }

    switch ((nav_t)t->nav) {
    case NAV_NEXT:
    case NAV_SKIP:
        playlist_next(f->pl);
        break;
    case NAV_PREV:
        playlist_prev(f->pl);
        break;
    case NAV_AUTO:
        if (playlist_auto_advance(f->pl))
            act |= FSM_ACT_WRAPPED;
        break;
    case NAV_NONE:
        break;
    }
    if (t->next != f->state) {
        f->state = (fsm_state_t)t->next;
        act |= FSM_ACT_STATE;
    }
    return act;
}

const char *fsm_state_name(fsm_state_t s)
{
    switch (s) {
    case FSM_STOPPED: return "DETENIDO";
    case FSM_PLAYING: return "REPRODUCIENDO";
    case FSM_PAUSED:  return "PAUSADO";
    default:          return "?";
    }
}
