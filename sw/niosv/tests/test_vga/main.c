/*
 * Prueba del driver de texto VGA y de la pantalla (HW-09, NV-11, PR-05) para
 * sim/questa/tb_vga.sv. Dibuja la pantalla del reproductor con los mismos datos
 * que tests/test_screen.c (asi el testbench compara contra esa referencia) y
 * avisa con LEDR = 0x0B0 (0x0C8 si la resolucion que reporta el IP no es 80 x 60).
 * Mientras avanza deja en LEDR la etapa (0x0A1..0x0A4) para diagnosticar.
 */
#include <stdint.h>
#include "system.h"
#include "pio.h"
#include "screen.h"
#include "vga_text.h"

static vga_text_t vga;

static void vga_port_print(void *ctx, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    vga_text_print((const vga_text_t *)ctx, x, y, s, width);
}

int main(void)
{
    pio_write(PIO_LED_BASE, 0x0A1);
    vga_text_init(&vga, CHAR_BUF_AVALON_CHAR_CONTROL_SLAVE_BASE,
                  CHAR_BUF_AVALON_CHAR_BUFFER_SLAVE_BASE);

    screen_port_t port = { &vga, vga_port_print };
    screen_track_t t = { "Cancion de prueba", "Artista de prueba",
                         "Album con un nombre muy largo que no cabe en la pantalla del reproductor",
                         225, 3, 12 };
    pio_write(PIO_LED_BASE, 0x0A2);
    screen_draw_frame(&port);
    pio_write(PIO_LED_BASE, 0x0A3);
    screen_draw_track(&port, &t);
    pio_write(PIO_LED_BASE, 0x0A4);
    screen_draw_status(&port, SCREEN_PLAYING, "PASA-BAJOS (SW1)", 50, 72);

    /* Resolucion reportada por el IP (registro de control 0x4). */
    pio_write(PIO_LED_BASE, (vga.cols == 80 && vga.rows == 60) ? 0x0B0u : 0x0C8u);
    for (;;) {
    }
}
