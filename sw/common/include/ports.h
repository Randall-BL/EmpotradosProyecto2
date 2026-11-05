/*
 * Puertos hacia el hardware (SC-14).
 *
 * La logica del reproductor (app.c) no llama a drivers: recibe structs de punteros a
 * funcion. En el Nios V los implementa sw/niosv/src/main.c con los drivers sin HAL;
 * en el PC los implementan mocks (tests/test_app.c) que registran lo que se hizo. Asi
 * la logica completa (eventos, FSM, playlist, flujo de audio, pantalla) se prueba sin
 * tarjeta. Todos los punteros son obligatorios salvo que se diga lo contrario.
 */
#ifndef COMMON_PORTS_H
#define COMMON_PORTS_H

#include <stdint.h>
#include "screen.h"
#include "switches.h"

/* Salida de audio: FIFO del codec, con el filtro de hardware en medio (CD-04). */
typedef struct {
    void *ctx;
    uint32_t depth;                                   /* capacidad de la FIFO en cuadros */
    uint32_t (*space)(void *ctx);                     /* cuadros que caben sin esperar */
    void (*write)(void *ctx, uint32_t lr);            /* un cuadro, empaquetado L | R<<16 */
} audio_port_t;

/* Filtro y volumen del modulo propio (CD-04, CD-05). */
typedef struct {
    void *ctx;
    void (*set_filter)(void *ctx, filter_id_t f);     /* FILTER_REVERB = bypass en HW */
    void (*set_gain)(void *ctx, uint32_t gain_q15);
} fx_port_t;

/* Reloj en milisegundos (NV-12). */
typedef struct {
    void *ctx;
    uint32_t (*now_ms)(void *ctx);
} clock_port_t;

/* Indicadores: 7 segmentos (tiempo y numero de pista) y LEDs. */
typedef struct {
    void *ctx;
    void (*set_time)(void *ctx, uint32_t seconds);
    void (*set_track)(void *ctx, uint32_t number);    /* 1..99, 0 = apagado */
    void (*set_leds)(void *ctx, uint32_t leds);
} panel_port_t;

/* Entradas leidas por encuesta: switches y potenciometro. */
typedef struct {
    void *ctx;
    uint32_t (*switches)(void *ctx);                  /* SW[9:0] */
    uint32_t (*volume_adc)(void *ctx);                /* 0..4095 */
} input_port_t;

/* Datos de una pista para la pantalla. Los textos los mantiene la fuente. */
typedef struct {
    const char *title;
    const char *artist;
    const char *album;
    uint32_t duration_s;
} track_meta_t;

/* Estado de la fuente (SRC_*). */
#define SRC_WAITING  0u     /* la fuente todavia no tiene la tabla (HPS arrancando) */
#define SRC_IDLE     1u     /* lista para recibir comandos */
#define SRC_LOADING  2u     /* se pidio una pista y no hay respuesta todavia */
#define SRC_RUNNING  3u     /* entregando cuadros */
#define SRC_ENDED    4u     /* termino la pista y ya se consumio todo */
#define SRC_ERROR    5u     /* la pista no se pudo leer */
#define SRC_FAILED   6u     /* la fuente no puede funcionar (sin SD, version) */

/* Fuente de audio y de la tabla de pistas: el HPS por memoria compartida (NV-13). */
typedef struct {
    void *ctx;
    uint32_t (*state)(void *ctx);                     /* SRC_* */
    uint32_t (*track_count)(void *ctx);
    int (*track_meta)(void *ctx, uint32_t index, track_meta_t *out);  /* 0 = ok */
    void (*play)(void *ctx, uint32_t index);          /* desde el inicio */
    void (*stop)(void *ctx);
    uint32_t (*read)(void *ctx, uint32_t *frames, uint32_t max);
} source_port_t;

typedef struct {
    audio_port_t audio;
    fx_port_t fx;
    clock_port_t clock;
    panel_port_t panel;
    input_port_t input;
    source_port_t source;
    screen_port_t screen;
} app_ports_t;

#endif /* COMMON_PORTS_H */
