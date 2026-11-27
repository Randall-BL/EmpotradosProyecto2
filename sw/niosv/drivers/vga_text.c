/* Texto sobre el Character Buffer del University Program (NV-11). Ver vga_text.h. */
#include "vga_text.h"
#include "hw.h"

/* Registros del esclavo de control. */
#define CB_REG_CONTROL    0x0
#define CB_REG_RESOLUTION 0x4
#define CB_CTRL_CLEAR     (1u << 16)   /* escribir 1 borra; lee 1 mientras borra */

/* Direccion de la celda (x, y): cada fila ocupa 128 bytes (2^7), aunque solo se vean
 * 80 columnas. */
static inline uintptr_t cell(const vga_text_t *t, uint32_t x, uint32_t y)
{
    return t->chars + ((uintptr_t)y << 7) + x;
}

/* Guarda las bases, lee la resolucion del IP (con respaldo a 80 x 60) y espera a que
 * termine el borrado que el IP hace al salir del reset. */
void vga_text_init(vga_text_t *t, uintptr_t ctrl, uintptr_t chars)
{
    t->ctrl = ctrl;
    t->chars = chars;
    uint32_t res = HW_REG(ctrl, CB_REG_RESOLUTION);
    t->cols = res & 0xFFFFu;
    t->rows = res >> 16;
    /* Si el IP no reporta resolucion (valor 0), se usa la de CD-09. */
    if (t->cols == 0 || t->cols > VGA_TEXT_COLS)
        t->cols = VGA_TEXT_COLS;
    if (t->rows == 0 || t->rows > VGA_TEXT_ROWS)
        t->rows = VGA_TEXT_ROWS;
    /* El IP sale del reset borrando (R = 1): basta esperar a que termine. */
    while (HW_REG(ctrl, CB_REG_CONTROL) & CB_CTRL_CLEAR) {
    }
}

/* Pide el borrado de toda la pantalla y espera a que el IP termine. */
void vga_text_clear(const vga_text_t *t)
{
    HW_REG(t->ctrl, CB_REG_CONTROL) = CB_CTRL_CLEAR;
    while (HW_REG(t->ctrl, CB_REG_CONTROL) & CB_CTRL_CLEAR) {
    }
}

/* Escribe el caracter c en (x, y); fuera de la pantalla no hace nada. */
void vga_text_put(const vga_text_t *t, uint32_t x, uint32_t y, char c)
{
    if (x < t->cols && y < t->rows)
        HW_REG8(cell(t, x, y), 0) = (uint8_t)c;
}

/* Escribe s desde (x, y) sin pasar del borde derecho y despues rellena con espacios
 * hasta completar width celdas (asi un texto mas corto borra el anterior). */
void vga_text_print(const vga_text_t *t, uint32_t x, uint32_t y, const char *s, uint32_t width)
{
    if (y >= t->rows)
        return;
    uint32_t i = 0;
    for (; s[i] != '\0' && x + i < t->cols; ++i)
        HW_REG8(cell(t, x + i, y), 0) = (uint8_t)s[i];
    for (; i < width && x + i < t->cols; ++i)
        HW_REG8(cell(t, x + i, y), 0) = ' ';
}
