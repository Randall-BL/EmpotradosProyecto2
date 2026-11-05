/*
 * Driver de texto sobre el Character Buffer for VGA Display del University Program
 * (80 x 60 caracteres en 640 x 480, CD-09) (NV-11). Sin HAL.
 *
 * Interfaces ("Video IP Cores for Intel DE-Series Boards", seccion 3.5):
 *   control (CHAR_BUF_AVALON_CHAR_CONTROL_SLAVE_BASE)
 *     0x0 control    bit16 R: escribir 1 borra la pantalla; vuelve a 0 al terminar.
 *                    Vale 1 al salir del reset: el IP se borra solo al arrancar.
 *     0x4 resolution [15:0] columnas, [31:16] filas
 *   caracteres (CHAR_BUF_AVALON_CHAR_BUFFER_SLAVE_BASE), un byte ASCII por celda
 *     direccion = base + (y << 7) + x
 *
 * Solo escritura: el esclavo de caracteres mete espera solo en la primera de varias
 * lecturas seguidas, y el adaptador de ancho del bus (maestro de 32 bits, esclavo de
 * 8) siempre lee 4 bytes seguidos; las lecturas devuelven el dato de la direccion
 * anterior (visto en simulacion). Si la aplicacion necesita saber que hay en pantalla,
 * lo guarda en una copia en RAM.
 */
#ifndef DRIVERS_VGA_TEXT_H
#define DRIVERS_VGA_TEXT_H

#include <stdint.h>

#define VGA_TEXT_COLS 80u
#define VGA_TEXT_ROWS 60u

typedef struct {
    uintptr_t ctrl;     /* base del esclavo de control */
    uintptr_t chars;    /* base del esclavo de caracteres */
    uint32_t  cols;
    uint32_t  rows;
} vga_text_t;

/* Lee la resolucion del IP y espera a que termine el borrado del arranque. Hay que
 * llamarla antes de escribir: lo que se escriba mientras el borrado esta en curso
 * se pierde en las celdas por las que aun no paso. Tarda hasta ~33 ms. */
void vga_text_init(vga_text_t *t, uintptr_t ctrl, uintptr_t chars);

/* Borra la pantalla y espera a que el IP termine. Ojo: a diferencia de lo que dice
 * la guia del IP ("5000 ciclos"), el borrado se hace al ritmo del video: espera
 * el inicio de un cuadro y recorre el cuadro completo, de 16.7 a 33 ms (verificado
 * en altera_up_avalon_video_character_buffer_with_dma). Para actualizar campos se
 * reescriben con espacios de relleno (vga_text_print con width), no se borra. */
void vga_text_clear(const vga_text_t *t);

void vga_text_put(const vga_text_t *t, uint32_t x, uint32_t y, char c);

/* Escribe s desde (x, y) y rellena con espacios hasta completar width celdas
 * (width = 0: solo el texto). Recorta en el borde derecho. */
void vga_text_print(const vga_text_t *t, uint32_t x, uint32_t y, const char *s, uint32_t width);


#endif /* DRIVERS_VGA_TEXT_H */
