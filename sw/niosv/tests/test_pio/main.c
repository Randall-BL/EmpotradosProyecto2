/*
 * Prueba de los PIO del SoC (HW-02, PR-05) para el testbench sim/questa/tb_pio.sv.
 *
 * El programa y el testbench se sincronizan por los LEDs; cada etapa deja en
 * LEDR un valor que el testbench espera antes de dar el siguiente estimulo:
 *
 *   1. LEDR = 0x2AA                 escritura de salidas
 *   2. LEDR = SW                    lectura de entradas (el tb pone SW = 0x155);
 *                                   sigue a los switches hasta que todos bajen
 *   3. LEDR = 0x300                 IRQ de KEY y SW registradas y habilitadas
 *   4. LEDR = 0x100 | flancos KEY   en cada interrupcion de los botones
 *      LEDR = 0x200 | SW[8:0]       en cada interrupcion de los switches
 *
 * Los cambios de etapa usan el valor de SW ya estable (sin cambios durante
 * SW_SETTLE_MS): en la tarjeta los switches rebotan al moverlos, y sin esto un
 * rebote a 0 adelantaba las etapas 2 y 3 (p. ej. al subir solo SW8 quedaba
 * LEDR = 0x200). En el testbench los switches no rebotan; solo agrega la espera.
 *
 * Sin printf: la JTAG UART simulada es lenta y alargaria la simulacion.
 */
#include <stdint.h>
#include "system.h"
#include "pio.h"
#include "irq.h"
#include "timebase.h"

#define LED_STAGE_OUT   0x2AAu
#define LED_STAGE_READY 0x300u
#define LED_KEY_EVENT   0x100u
#define LED_SW_EVENT    0x200u

#define SW_MASK         0x3FFu
#define SW_SETTLE_MS    20u     /* mayor que el rebote tipico de un switch (< 10 ms) */
#define SW_SETTLE_CYC   (SW_SETTLE_MS * (ALT_CPU_CPU_FREQ / 1000u))

static volatile uint32_t key_events;
static volatile uint32_t sw_events;

/* ISR cortas: solo leen y limpian la fuente y dejan el evento al lazo principal. */
static void key_isr(void *ctx)
{
    (void)ctx;
    key_events |= pio_edges_take(PIO_KEY_BASE);
}

static void sw_isr(void *ctx)
{
    (void)ctx;
    sw_events |= pio_edges_take(PIO_SW_BASE);
}

/* Devuelve SW cuando lleva SW_SETTLE_MS sin cambiar (antirrebote por espera). */
static uint32_t sw_read_stable(void)
{
    uint32_t value = pio_read(PIO_SW_BASE) & SW_MASK;
    uint32_t since = timebase_cycles();

    while ((uint32_t)(timebase_cycles() - since) < SW_SETTLE_CYC) {
        uint32_t now = pio_read(PIO_SW_BASE) & SW_MASK;
        if (now != value) {
            value = now;
            since = timebase_cycles();
        }
    }
    return value;
}

int main(void)
{
    uint32_t sw;

    /* Etapa 1: salidas. */
    pio_write(PIO_LED_BASE, LED_STAGE_OUT);
    while ((sw = sw_read_stable()) == 0) {
    }

    /* Etapa 2: entradas. LEDR sigue a SW hasta que todos los switches bajen. */
    do {
        pio_write(PIO_LED_BASE, sw);
        sw = sw_read_stable();
    } while (sw != 0);

    /* Etapa 3: interrupciones. Se limpian flancos viejos antes de habilitar. */
    (void)pio_edges_take(PIO_KEY_BASE);
    (void)pio_edges_take(PIO_SW_BASE);
    irq_attach(PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID, PIO_KEY_IRQ, key_isr, 0);
    irq_attach(PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID, PIO_SW_IRQ, sw_isr, 0);
    pio_irq_mask(PIO_KEY_BASE, 0xFu);
    pio_irq_mask(PIO_SW_BASE, 0x3FFu);
    pio_write(PIO_LED_BASE, LED_STAGE_READY);

    /* Etapa 4: eventos. */
    for (;;) {
        if (key_events != 0) {
            irq_disable(PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID, PIO_KEY_IRQ);
            uint32_t ev = key_events;
            key_events = 0;
            irq_enable(PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID, PIO_KEY_IRQ);
            pio_write(PIO_LED_BASE, LED_KEY_EVENT | (ev & 0xFu));
        }
        if (sw_events != 0) {
            sw_events = 0;
            pio_write(PIO_LED_BASE, LED_SW_EVENT | (pio_read(PIO_SW_BASE) & 0x1FFu));
        }
    }
}
