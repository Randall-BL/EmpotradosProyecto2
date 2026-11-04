/*
 * Pruebas de la playlist (SC-07) y de la maquina de estados (SC-08).
 */
#include "fsm.h"
#include "playlist.h"
#include "test.h"

static void test_playlist(void)
{
    playlist_t p;
    CHECK_EQ(playlist_init(&p, 0), ERR_EMPTY);
    CHECK_EQ(playlist_number(&p), 0);
    CHECK_EQ(playlist_next(&p), 0);            /* lista vacia: no se mueve */
    CHECK_EQ(playlist_auto_advance(&p), 0);
    CHECK_EQ(playlist_init(NULL, 3), ERR_ARG);

    CHECK_EQ(playlist_init(&p, 12), ERR_OK);
    CHECK(playlist_meets_minimum(&p));
    CHECK_EQ(playlist_number(&p), 1);
    CHECK_EQ(playlist_prev(&p), 11);           /* anterior desde la primera: la ultima */
    CHECK_EQ(playlist_number(&p), 12);
    CHECK_EQ(playlist_next(&p), 0);            /* siguiente desde la ultima: la primera */
    for (int i = 0; i < 11; ++i)
        CHECK_EQ(playlist_auto_advance(&p), 0);
    CHECK_EQ(p.current, 11);
    CHECK_EQ(playlist_auto_advance(&p), 1);    /* dio la vuelta */
    CHECK_EQ(p.current, 0);
    CHECK_EQ(playlist_select(&p, 5), ERR_OK);
    CHECK_EQ(playlist_number(&p), 6);
    CHECK_EQ(playlist_select(&p, 12), ERR_BOUNDS);
    CHECK_EQ(p.current, 5);

    CHECK_EQ(playlist_init(&p, 9), ERR_OK);
    CHECK(!playlist_meets_minimum(&p));
    CHECK_EQ(playlist_init(&p, 1), ERR_OK);    /* una sola pista: siempre la misma */
    CHECK_EQ(playlist_next(&p), 0);
    CHECK_EQ(playlist_prev(&p), 0);
    CHECK_EQ(playlist_auto_advance(&p), 1);
}

static void test_basic_flow(void)
{
    playlist_t p;
    fsm_t f;
    playlist_init(&p, 10);
    fsm_init(&f, &p);
    CHECK_EQ(f.state, FSM_STOPPED);

    uint32_t a = fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    CHECK_EQ(f.state, FSM_PLAYING);
    CHECK_EQ(a, FSM_ACT_LOAD | FSM_ACT_START | FSM_ACT_STATE);

    a = fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    CHECK_EQ(f.state, FSM_PAUSED);
    CHECK_EQ(a, FSM_ACT_PAUSE | FSM_ACT_STATE);

    a = fsm_handle(&f, FSM_EV_PLAY_PAUSE);     /* reanuda sin recargar */
    CHECK_EQ(f.state, FSM_PLAYING);
    CHECK_EQ(a, FSM_ACT_START | FSM_ACT_STATE);

    a = fsm_handle(&f, FSM_EV_STOP);
    CHECK_EQ(f.state, FSM_STOPPED);
    CHECK_EQ(a, FSM_ACT_STOP | FSM_ACT_STATE);
    CHECK_EQ(p.current, 0);                    /* stop no cambia de pista */

    CHECK_EQ(fsm_handle(&f, FSM_EV_STOP), 0);  /* stop detenido: nada */
    CHECK_EQ(fsm_handle(&f, FSM_EV_TRACK_END), 0);
    CHECK_EQ(fsm_handle(&f, FSM_EV_ERROR), 0);
}

static void test_navigation(void)
{
    playlist_t p;
    fsm_t f;
    playlist_init(&p, 10);
    fsm_init(&f, &p);

    /* Detenido: cambia la seleccion sin reproducir. */
    CHECK_EQ(fsm_handle(&f, FSM_EV_PREV), FSM_ACT_TRACK);
    CHECK_EQ(p.current, 9);
    CHECK_EQ(fsm_handle(&f, FSM_EV_NEXT), FSM_ACT_TRACK);
    CHECK_EQ(p.current, 0);
    CHECK_EQ(f.state, FSM_STOPPED);

    /* Reproduciendo: cambia y reproduce. */
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    CHECK_EQ(fsm_handle(&f, FSM_EV_NEXT), FSM_ACT_TRACK | FSM_ACT_LOAD | FSM_ACT_START);
    CHECK_EQ(p.current, 1);
    CHECK_EQ(f.state, FSM_PLAYING);

    /* Pausado: cambia, carga y sigue en pausa. */
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    CHECK_EQ(fsm_handle(&f, FSM_EV_PREV), FSM_ACT_TRACK | FSM_ACT_LOAD);
    CHECK_EQ(p.current, 0);
    CHECK_EQ(f.state, FSM_PAUSED);
    CHECK_EQ(fsm_handle(&f, FSM_EV_TRACK_END), 0);     /* no puede terminar en pausa */
}

static void test_auto_advance(void)
{
    playlist_t p;
    fsm_t f;
    playlist_init(&p, 10);
    fsm_init(&f, &p);
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    int wraps = 0;
    /* Dos vueltas completas (PR-08): 20 fines de pista. */
    for (int i = 0; i < 20; ++i) {
        uint32_t a = fsm_handle(&f, FSM_EV_TRACK_END);
        CHECK(a & FSM_ACT_LOAD);
        CHECK(a & FSM_ACT_START);
        CHECK(a & FSM_ACT_TRACK);
        if (a & FSM_ACT_WRAPPED)
            wraps++;
        CHECK_EQ(f.state, FSM_PLAYING);
    }
    CHECK_EQ(wraps, 2);
    CHECK_EQ(p.current, 0);
}

static void test_errors(void)
{
    playlist_t p;
    fsm_t f;
    playlist_init(&p, 4);
    fsm_init(&f, &p);
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);

    /* Una pista mala: se salta a la siguiente. */
    uint32_t a = fsm_handle(&f, FSM_EV_ERROR);
    CHECK_EQ(a, FSM_ACT_TRACK | FSM_ACT_LOAD | FSM_ACT_START);
    CHECK_EQ(p.current, 1);
    CHECK_EQ(fsm_handle(&f, FSM_EV_TRACK_END) & FSM_ACT_LOAD, FSM_ACT_LOAD);  /* reinicia */
    CHECK_EQ(f.errors, 0);

    /* Todas malas: se detiene despues de count errores seguidos. */
    for (int i = 0; i < 3; ++i)
        CHECK_EQ(fsm_handle(&f, FSM_EV_ERROR) & FSM_ACT_ALL_FAILED, 0);
    a = fsm_handle(&f, FSM_EV_ERROR);
    CHECK(a & FSM_ACT_ALL_FAILED);
    CHECK(a & FSM_ACT_STOP);
    CHECK_EQ(f.state, FSM_STOPPED);

    /* Una accion del usuario reinicia la cuenta. */
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    fsm_handle(&f, FSM_EV_ERROR);
    fsm_handle(&f, FSM_EV_ERROR);
    fsm_handle(&f, FSM_EV_NEXT);
    CHECK_EQ(f.errors, 0);

    /* Error en pausa: salta y sigue en pausa. */
    fsm_handle(&f, FSM_EV_PLAY_PAUSE);
    CHECK_EQ(f.state, FSM_PAUSED);
    uint32_t before = p.current;
    CHECK_EQ(fsm_handle(&f, FSM_EV_ERROR), FSM_ACT_TRACK | FSM_ACT_LOAD);
    CHECK_EQ(p.current, (before + 1) % 4);
}

static void test_robustness(void)
{
    playlist_t p;
    fsm_t f;
    playlist_init(&p, 0);
    fsm_init(&f, &p);
    CHECK_EQ(fsm_handle(&f, FSM_EV_PLAY_PAUSE), 0);    /* lista vacia */
    CHECK_EQ(f.state, FSM_STOPPED);
    playlist_init(&p, 3);
    CHECK_EQ(fsm_handle(&f, (fsm_event_t)99), 0);
    CHECK_STR(fsm_state_name(FSM_PAUSED), "PAUSADO");
    CHECK_STR(fsm_state_name((fsm_state_t)7), "?");

    /* Cualquier secuencia de eventos deja un estado valido y una pista valida. */
    unsigned seed = 12345;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1103515245u + 12345u;
        fsm_handle(&f, (fsm_event_t)((seed >> 16) % FSM_NUM_EVENTS));
        CHECK(f.state < FSM_NUM_STATES);
        CHECK(p.current < p.count);
    }
}

int main(void)
{
    RUN(test_playlist);
    RUN(test_basic_flow);
    RUN(test_navigation);
    RUN(test_auto_advance);
    RUN(test_errors);
    RUN(test_robustness);
    return TEST_REPORT();
}
