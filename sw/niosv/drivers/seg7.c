/*
 * Driver del modulo seg7_time (NV-09). Ver seg7.h y hw/rtl/seg7_time.v.
 * El hardware hace la conversion de segundos a MM:SS y a 7 segmentos; el software
 * solo escribe numeros.
 */
#include "seg7.h"
#include "hw.h"

/* Desplazamientos de los registros de seg7_time. */
#define S7_CTRL   0x0   /* encendido y visibilidad del numero de pista */
#define S7_TIME   0x4   /* segundos transcurridos */
#define S7_TRACK  0x8   /* numero de pista (0..99) */
#define S7_STATUS 0xC   /* BCD {M1,M0,S1,S0} que se esta mostrando */

#define CTRL_ON    (1u << 0)   /* 1 = displays encendidos */
#define CTRL_TRACK (1u << 1)   /* 1 = mostrar la pista en HEX5..HEX4 */

/* Copia del registro de control para cambiar un bit sin leerlo. */
static uint32_t ctrl_shadow = CTRL_ON;

/* Enciende los displays con 00:00 y sin numero de pista. */
void seg7_init(uintptr_t base)
{
    ctrl_shadow = CTRL_ON;
    HW_REG(base, S7_CTRL) = ctrl_shadow;
    seg7_set_time(base, 0);
}

/* Muestra seconds como MM:SS (el hardware satura en 99:59). */
void seg7_set_time(uintptr_t base, uint32_t seconds)
{
    HW_REG(base, S7_TIME) = seconds;
}

/* Escribe el numero de pista y lo muestra (show != 0) u oculta. */
void seg7_set_track(uintptr_t base, uint32_t track, int show)
{
    HW_REG(base, S7_TRACK) = track;
    ctrl_shadow = show ? (ctrl_shadow | CTRL_TRACK) : (ctrl_shadow & ~CTRL_TRACK);
    HW_REG(base, S7_CTRL) = ctrl_shadow;
}

/* Enciende (on != 0) o apaga todos los displays. */
void seg7_enable(uintptr_t base, int on)
{
    ctrl_shadow = on ? (ctrl_shadow | CTRL_ON) : (ctrl_shadow & ~CTRL_ON);
    HW_REG(base, S7_CTRL) = ctrl_shadow;
}

/* Lee los 4 digitos BCD que muestra el hardware (para las pruebas). */
uint32_t seg7_shown_bcd(uintptr_t base)
{
    return HW_REG(base, S7_STATUS) & 0xFFFFu;
}
