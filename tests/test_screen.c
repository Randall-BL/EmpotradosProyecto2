/*
 * Pruebas en el PC de la pantalla del reproductor (sw/common/screen.c, NV-11).
 * Dibuja en una matriz de 80 x 13 y compara con el boceto de CD-09.
 * Con -v imprime la pantalla.  Compilar y correr:  make -C tests
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screen.h"

static char grid[SCREEN_ROWS][SCREEN_COLS + 1];
static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void grid_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    (void)ctx;
    if (y >= SCREEN_ROWS)
        return;
    uint32_t i = 0;
    for (; s[i] && x + i < SCREEN_COLS; ++i)
        grid[y][x + i] = s[i];
    for (; i < width && x + i < SCREEN_COLS; ++i)
        grid[y][x + i] = ' ';
}

static int starts(uint32_t y, uint32_t x, const char *s)
{
    return strncmp(&grid[y][x], s, strlen(s)) == 0;
}

int main(int argc, char **argv)
{
    char b[16];
    screen_fmt_mmss(b, 0);      CHECK(strcmp(b, "00:00") == 0);
    screen_fmt_mmss(b, 225);    CHECK(strcmp(b, "03:45") == 0);
    screen_fmt_mmss(b, 3599);   CHECK(strcmp(b, "59:59") == 0);
    screen_fmt_mmss(b, 999999); CHECK(strcmp(b, "99:59") == 0);
    screen_fmt_uint(b, 7, 3, ' ');   CHECK(strcmp(b, "  7") == 0);
    screen_fmt_uint(b, 100, 3, ' '); CHECK(strcmp(b, "100") == 0);
    screen_fmt_uint(b, 3, 2, '0');   CHECK(strcmp(b, "03") == 0);
    screen_fmt_uint(b, 0, 1, ' ');   CHECK(strcmp(b, "0") == 0);

    for (uint32_t y = 0; y < SCREEN_ROWS; ++y) {
        memset(grid[y], '.', SCREEN_COLS);
        grid[y][SCREEN_COLS] = '\0';
    }
    screen_port_t port = { NULL, grid_print };
    screen_track_t t = { "Cancion de prueba", "Artista de prueba",
                         "Album con un nombre muy largo que no cabe en la pantalla del reproductor",
                         225, 3, 12 };
    screen_draw_frame(&port);
    screen_draw_track(&port, &t);
    screen_draw_status(&port, SCREEN_PLAYING, "PASA-BAJOS (SW1)", 50, 72);

    if (argc > 1 && strcmp(argv[1], "-v") == 0)
        for (uint32_t y = 0; y < SCREEN_ROWS; ++y)
            printf("%s\n", grid[y]);

    /* Toda celda queda escrita (no queda ningun '.' de relleno inicial). */
    for (uint32_t y = 0; y < SCREEN_ROWS; ++y)
        CHECK(strchr(grid[y], '.') == NULL);
    CHECK(grid[0][0] == '+' && grid[0][79] == '+' && grid[12][0] == '+');
    CHECK(grid[4][0] == '|' && grid[4][79] == '|');
    CHECK(starts(1, 2, "REPRODUCTOR DE AUDIO - CE1113"));
    CHECK(starts(1, 60, "Cancion   3 / 12"));
    CHECK(starts(3, 2, "Titulo  : Cancion de prueba"));
    CHECK(starts(4, 2, "Artista : Artista de prueba "));
    CHECK(starts(5, 2, "Album   : Album con un nombre muy largo que no cabe en la   ")); /* 48 car. */
    CHECK(starts(6, 2, "Duracion: 03:45"));
    CHECK(starts(6, 35, "Transcurrido: 01:12"));
    CHECK(starts(8, 2, "Estado  : >  REPRODUCIENDO"));
    CHECK(starts(8, 35, "Filtro: PASA-BAJOS (SW1)"));
    CHECK(starts(9, 2, "Volumen : [##########----------]  50%"));

    /* Cambio de estado: los textos mas cortos borran los anteriores. */
    screen_draw_status(&port, SCREEN_PAUSED, "BYPASS", 100, 0);
    CHECK(starts(8, 2, "Estado  : || PAUSADO      "));
    CHECK(starts(8, 35, "Filtro: BYPASS            "));
    CHECK(starts(9, 2, "Volumen : [####################] 100%"));
    CHECK(grid[8][79] == '|');

    if (failures) {
        printf("test_screen: FAIL (%d)\n", failures);
        return EXIT_FAILURE;
    }
    printf("test_screen: PASS\n");
    return EXIT_SUCCESS;
}
