/*
 * Aplicacion del reproductor de audio en el Nios V/m (NV-14).
 *
 * Arranque: drivers sin HAL -> autoprueba del modulo propio (NV-15) -> codec ->
 * botones por IRQ (NV-03) -> fuente sobre la memoria compartida con el HPS (NV-13) ->
 * lazo principal portable (sw/common/src/app.c), que se llama sin fin.
 *
 * Toda la logica (FSM, playlist, flujo de audio, filtros, volumen, pantalla) esta en
 * sw/common y se prueba en el PC (tests/test_app.c). Aqui solo se conectan los
 * puertos con los drivers (ports_niosv.c). Mapa de controles y LEDs: app.h y CD-08.
 */
#include <stdint.h>
#include "system.h"
#include "adc.h"
#include "app.h"
#include "audio.h"
#include "codec.h"
#include "evq.h"
#include "filtro.h"
#include "keys.h"
#include "pio.h"
#include "ports_niosv.h"
#include "seg7.h"
#include "shm_source.h"
#include "timebase.h"
#include "vga_text.h"

#define CODEC_TIMEOUT 20000000u              /* ~0.4 s de espera del I2C */

/* Estado global en .bss (sin malloc): ~10 KB, casi todo la reverberacion. */
static app_t app;
static evq_t events;
static shm_source_t source;
static vga_text_t vga;
static int16_t rev_l[APP_REVERB_FRAMES], rev_r[APP_REVERB_FRAMES];

/* Falla que impide reproducir: se muestra y se queda con LEDR9 parpadeando. */
static void fatal(const char *msg)
{
    vga_text_print(&vga, 2, 7, msg, 76);
    for (uint32_t n = 0;; ++n) {
        pio_write(PIO_LED_BASE, (n & 0x40000u) ? APP_LED_ALERT : 0u);
    }
}

int main(void)
{
    pio_write(PIO_LED_BASE, 0);
    timebase_init(0);
    vga_text_init(&vga, CHAR_BUF_AVALON_CHAR_CONTROL_SLAVE_BASE,
                  CHAR_BUF_AVALON_CHAR_BUFFER_SLAVE_BASE);
    vga_text_clear(&vga);
    seg7_init(SEG7_BASE);
    adc_init(ADC_BASE);
    audio_init(AUDIO_BASE);

    /* NV-15: rutina de prueba del modulo propio antes de usarlo. */
    filtro_init(FILTRO_BASE);
    uint32_t bad = filtro_selftest(FILTRO_BASE);
    filtro_init(FILTRO_BASE);
    if (codec_init(AV_CONFIG_BASE, CODEC_TIMEOUT) != CODEC_OK)
        fatal("ERROR: el codec WM8731 no respondio por I2C");

    evq_init(&events);
    if (keys_init(PIO_KEY_BASE, PIO_KEY_IRQ_INTERRUPT_CONTROLLER_ID, PIO_KEY_IRQ, &events) != 0)
        fatal("ERROR: no se pudo registrar la interrupcion de los botones");

    shm_source_init(&source, (volatile shm_t *)SHM_BASE);
    app_ports_t ports;
    ports_niosv_init(&ports, &vga);
    shm_source_port(&source, &ports.source);
    app_init(&app, &ports, &events, rev_l, rev_r);
    if (bad)
        vga_text_print(&vga, 0, SCREEN_ROWS + 1u, "AVISO: la autoprueba del filtro fallo (NV-15)", 60);

    for (;;)
        app_step(&app);
}
