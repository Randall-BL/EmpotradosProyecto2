/*
 * Mediciones de ciclos en el Nios V/m (CD-04, CD-05, CD-12, RP-04) para
 * sim/questa/tb_ciclos.sv. Cada operacion se repite REPS veces y se divide el conteo
 * de mtime; se descuenta el costo del lazo vacio. El Nios V/m no implementa el CSR
 * mcycle (leerlo provoca una excepcion), pero mtime cuenta a la frecuencia del CPU
 * (MTIMECMP_DELTA_AMT = ALT_CPU_CPU_FREQ / ticks): se lee por puntero, sin HAL. Los resultados salen por la JTAG
 * UART (printf), que el testbench copia al transcript.
 */
#include <stdint.h>
#include <stdio.h>
#include "system.h"
#include "dsp.h"
#include "fir_coefs.h"
#include "hw.h"
#include "pio.h"
#include "volume.h"

#define REPS 4u

static inline uint32_t cycles(void)
{
    return HW_REG(ALT_CPU_MTIME_OFFSET, 0);     /* palabra baja de mtime */
}

static volatile int16_t sink;
static volatile int16_t src = 12345;
static int16_t reverb_buf[256];

#define MEASURE(var, body)                              \
    do {                                                \
        uint32_t t0_ = cycles();                        \
        for (uint32_t i_ = 0; i_ < REPS; ++i_) { body; } \
        var = (cycles() - t0_) / REPS;                  \
    } while (0)

int main(void)
{
    uint32_t empty, gain, fir, biquad, reverb, mmio_w, mmio_r, shm_r;
    dsp_fir_t f;
    dsp_biquad_t q;
    dsp_reverb_t r;
    dsp_fir_init(&f, fir_coefs[FIR_BANK_LOWPASS], FIR_NTAPS);
    dsp_biquad_init(&q, 4096, 8192, 4096, -16000, 6000);
    dsp_reverb_init(&r, reverb_buf, 256);
    volatile uint32_t *shm = (volatile uint32_t *)SHM_BASE;

    /* LEDR = 0x0Fx: etapa x en curso (para diagnosticar si algo se cuelga). */
    pio_write(PIO_LED_BASE, 0x0F1);
    MEASURE(empty,  sink = src);
    pio_write(PIO_LED_BASE, 0x0F2);
    MEASURE(gain,   sink = volume_scale(src, 23170));
    pio_write(PIO_LED_BASE, 0x0F3);
    MEASURE(fir,    sink = dsp_fir_step(&f, src, 0));
    pio_write(PIO_LED_BASE, 0x0F4);
    MEASURE(biquad, sink = dsp_biquad_step(&q, src));
    pio_write(PIO_LED_BASE, 0x0F5);
    MEASURE(reverb, sink = dsp_reverb_step(&r, src));
    MEASURE(mmio_w, HW_REG(PIO_LED_BASE, 0) = 0x0F6);
    MEASURE(mmio_r, sink = (int16_t)HW_REG(PIO_SW_BASE, 0));
    MEASURE(shm_r,  sink = (int16_t)shm[3]);
    pio_write(PIO_LED_BASE, 0x0F7);

    printf("CICLOS lazo_vacio %lu\n", (unsigned long)empty);
    printf("CICLOS volumen_mul %lu\n", (unsigned long)(gain - empty));
    printf("CICLOS fir127 %lu\n", (unsigned long)(fir - empty));
    printf("CICLOS biquad %lu\n", (unsigned long)(biquad - empty));
    printf("CICLOS reverb %lu\n", (unsigned long)(reverb - empty));
    printf("CICLOS escritura_mmio %lu\n", (unsigned long)(mmio_w - empty));
    printf("CICLOS lectura_mmio %lu\n", (unsigned long)(mmio_r - empty));
    printf("CICLOS lectura_shm %lu\n", (unsigned long)(shm_r - empty));
    printf("CICLOS fin\n");
    pio_write(PIO_LED_BASE, 0x0E0);
    for (;;) {
    }
}
