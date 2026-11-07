/*
 * Co-simulacion HW/SW del modulo propio en el SoC (PR-04, NV-09, NV-10, NV-15) para
 * sim/questa/tb_filtro.sv:
 *   1. filtro_init carga los coeficientes por el bus.
 *   2. filtro_selftest compara 431 pares del hardware con la respuesta esperada.
 *   3. Mide los ciclos por par con pipelining (escribir el par i, leer el i-1).
 *   4. seg7: muestra la cancion 3 y 02:05, y relee el BCD mostrado.
 * Resultados por la JTAG UART ("FILTRO ...") y en LEDR: 0x0D0 todo bien, 0x0DF fallo.
 */
#include <stdint.h>
#include <stdio.h>
#include "system.h"
#include "filtro.h"
#include "hw.h"
#include "pio.h"
#include "seg7.h"

#define FRAMES 48u

static inline uint32_t cycles(void)
{
    return HW_REG(ALT_CPU_MTIME_OFFSET, 0);
}

int main(void)
{
    pio_write(PIO_LED_BASE, 0x0D1);
    filtro_init(FILTRO_BASE);

    pio_write(PIO_LED_BASE, 0x0D2);
    uint32_t bad = filtro_selftest(FILTRO_BASE);

    /* Ciclos por par con pipelining, filtro activo (banco 0, 127 taps). */
    pio_write(PIO_LED_BASE, 0x0D3);
    filtro_config(FILTRO_BASE, 0, FILTRO_BANK_LOWPASS);
    volatile uint32_t sink = 0;
    uint32_t t0 = cycles();
    filtro_write(FILTRO_BASE, filtro_pack(1000, -1000));
    for (uint32_t i = 1; i < FRAMES; ++i) {
        uint32_t y = filtro_read(FILTRO_BASE);
        filtro_write(FILTRO_BASE, filtro_pack((int16_t)i, (int16_t)-i));
        sink = y;
    }
    sink = filtro_read(FILTRO_BASE);
    uint32_t per_frame = (cycles() - t0) / FRAMES;
    (void)sink;

    /* Displays */
    seg7_init(SEG7_BASE);
    seg7_set_track(SEG7_BASE, 3, 1);
    seg7_set_time(SEG7_BASE, 125);
    uint32_t bcd;
    do {
        bcd = seg7_shown_bcd(SEG7_BASE);
    } while (bcd != 0x0205u && cycles() - t0 < 200000u);

    printf("FILTRO autoprueba_diferencias %lu\n", (unsigned long)bad);
    printf("FILTRO ciclos_por_par %lu\n", (unsigned long)per_frame);
    printf("FILTRO seg7_bcd %04lx\n", (unsigned long)bcd);
    pio_write(PIO_LED_BASE, (bad == 0 && bcd == 0x0205u) ? 0x0D0u : 0x0DFu);
    for (;;) {
    }
}
