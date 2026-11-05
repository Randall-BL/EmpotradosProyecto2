/*
 * Prueba de la memoria compartida HPS <-> Nios V vista desde el Nios V (HW-06,
 * PR-05) para sim/questa/tb_shm.sv.
 *
 * Tests clasicos de memoria (M. Barr, "Software-based memory testing"), que cubren
 * los 64 KB con pocos accesos:
 *   1. Bus de datos: un 1 que camina por los 32 bits de una palabra.
 *   2. Bus de direcciones: patrones en las direcciones 2^k (k = 0..13 palabras);
 *      una linea de direccion pegada a 0 o a 1, o dos cruzadas, hace que una
 *      escritura aparezca en otra direccion.
 *   3. March C- reducido en 64 palabras al inicio y al final (celdas).
 *   4. shm no se solapa con la RAM del programa.
 *   5. Accesos de 8 y 16 bits (byteenable; el protocolo SC-13 los usa).
 * Recorrer las 16 384 palabras tarda mas de 20 ms simulados (~30 ciclos por acceso
 * del Nios V/m), por eso no se hace.
 *
 * LEDR: 0x2A0 si todo bien, 0x2A1 mientras corre, 0x2Ex si fallo la prueba x.
 */
#include <stdint.h>
#include "system.h"
#include "pio.h"

#define WORDS       (SHM_SPAN / 4u)
#define MARCH_WORDS 64u
#define PATTERN     0xAAAAAAAAu
#define ANTIPATTERN 0x55555555u

static volatile uint32_t ram_marker = 0xC0FFEE00u;

static int data_bus(volatile uint32_t *m)
{
    for (uint32_t bit = 1; bit != 0; bit <<= 1) {
        *m = bit;
        if (*m != bit)
            return 1;
    }
    return 0;
}

static int address_bus(volatile uint32_t *m)
{
    for (uint32_t off = 1; off < WORDS; off <<= 1)
        m[off] = PATTERN;
    m[0] = ANTIPATTERN;
    /* Linea pegada a 1: escribir en 0 cambia alguna 2^k. */
    for (uint32_t off = 1; off < WORDS; off <<= 1)
        if (m[off] != PATTERN)
            return 1;
    /* Linea pegada a 0 o cruzada: escribir en 2^k cambia otra. */
    m[0] = PATTERN;
    for (uint32_t test = 1; test < WORDS; test <<= 1) {
        m[test] = ANTIPATTERN;
        if (m[0] != PATTERN)
            return 1;
        for (uint32_t off = 1; off < WORDS; off <<= 1)
            if (off != test && m[off] != PATTERN)
                return 1;
        m[test] = PATTERN;
    }
    return 0;
}

static int march(volatile uint32_t *m)
{
    for (uint32_t i = 0; i < MARCH_WORDS; ++i) m[i] = 0;
    for (uint32_t i = 0; i < MARCH_WORDS; ++i) {          /* subiendo: 0 -> 1s */
        if (m[i] != 0) return 1;
        m[i] = 0xFFFFFFFFu;
    }
    for (uint32_t i = MARCH_WORDS; i-- > 0;) {             /* bajando: 1s -> 0 */
        if (m[i] != 0xFFFFFFFFu) return 1;
        m[i] = 0;
    }
    for (uint32_t i = 0; i < MARCH_WORDS; ++i)
        if (m[i] != 0) return 1;
    return 0;
}

static uint32_t run(void)
{
    volatile uint32_t *m = (volatile uint32_t *)SHM_BASE;

    if (data_bus(m)) return 1;
    if (address_bus(m)) return 2;
    if (march(m) || march(m + WORDS - MARCH_WORDS)) return 3;
    if (ram_marker != 0xC0FFEE00u) return 4;

    volatile uint8_t  *b = (volatile uint8_t *)SHM_BASE;
    volatile uint16_t *h = (volatile uint16_t *)SHM_BASE;
    m[0] = 0;
    b[1] = 0xAB;
    h[1] = 0x1234;
    if (m[0] != 0x1234AB00u) return 5;
    return 0;
}

int main(void)
{
    pio_write(PIO_LED_BASE, 0x2A1);
    uint32_t r = run();
    pio_write(PIO_LED_BASE, r == 0 ? 0x2A0u : (0x2E0u | r));
    for (;;) {
    }
}
