/*
 * Pantalla del reproductor (NV-11, boceto de docs/decisiones/CD-09.md).
 *
 * Portable (sw/common/): no conoce el hardware. Dibuja a traves de un "puerto" de
 * texto (screen_port_t) que en el Nios V escribe en el Character Buffer
 * (vga_text) y en el PC escribe en una matriz para las pruebas
 * (tests/test_screen.c). Sin printf ni division por variable: formatea con
 * restas, porque el Nios V/m no tiene multiplicador ni divisor.
 *
 * Filas (80 columnas):
 *    0  +-----...-----+
 *    1  | REPRODUCTOR DE AUDIO - CE1113            Cancion  NN / NN |
 *    2  |-----...-----|
 *    3  | Titulo  : ...
 *    4  | Artista : ...
 *    5  | Album   : ...
 *    6  | Duracion: MM:SS                Transcurrido: MM:SS
 *    7  |
 *    8  | Estado  : ...                  Filtro: ...
 *    9  | Volumen : [####----...] NNN%
 *   10  |-----...-----|
 *   11  | KEY0 Play/Pausa  KEY1 Stop  KEY2 Anterior  KEY3 Siguiente   SW: filtro |
 *   12  +-----...-----+
 */
#ifndef COMMON_SCREEN_H
#define COMMON_SCREEN_H

#include <stdint.h>

#define SCREEN_COLS 80u
#define SCREEN_ROWS 13u              /* filas que usa la pantalla */
#define SCREEN_FIELD_MAX 48u         /* largo maximo mostrado de titulo/artista/album */

typedef struct {
    void *ctx;
    /* Escribe s en (x, y) y rellena con espacios hasta width celdas. */
    void (*print)(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width);
} screen_port_t;

typedef enum {
    SCREEN_STOPPED = 0,
    SCREEN_PLAYING,
    SCREEN_PAUSED
} screen_state_t;

typedef struct {
    const char *title;
    const char *artist;
    const char *album;
    uint32_t duration_s;
    uint32_t track;                  /* 1..total */
    uint32_t total;
} screen_track_t;

/* Marco, titulo de la aplicacion y ayuda de los controles (una vez). */
void screen_draw_frame(const screen_port_t *p);

/* Datos de la cancion (al cambiar de cancion). */
void screen_draw_track(const screen_port_t *p, const screen_track_t *t);

/* Estado, filtro, volumen y tiempo transcurrido (cada segundo o al cambiar). */
void screen_draw_status(const screen_port_t *p, screen_state_t st, const char *filter,
                        uint32_t volume_percent, uint32_t elapsed_s);

/* Utilidades de formato (expuestas para las pruebas). out debe tener 6 bytes. */
void screen_fmt_mmss(char *out, uint32_t seconds);
/* Entero 0..999 con ancho fijo width (relleno con espacios o ceros). */
void screen_fmt_uint(char *out, uint32_t v, uint32_t width, char pad);

#endif /* COMMON_SCREEN_H */
