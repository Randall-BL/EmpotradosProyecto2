#include "filtro.h"
#include "dsp.h"
#include "fir_coefs.h"
#include "hw.h"

#define F_CTRL      0x00
#define F_STATUS    0x04
#define F_NTAPS     0x08
#define F_GAIN      0x0C
#define F_COEF_ADDR 0x10
#define F_COEF_DATA 0x14
#define F_DATA_IN   0x18
#define F_DATA_OUT  0x1C

#define CTRL_BYPASS (1u << 0)
#define CTRL_BANK   (1u << 1)
#define CTRL_CLEAR  (1u << 2)
#define STATUS_BUSY (1u << 1)
#define STATUS_SAT  (1u << 2)

static uint32_t ctrl_shadow;

static void wait_idle(uintptr_t base)
{
    while (HW_REG(base, F_STATUS) & STATUS_BUSY) {
    }
}

void filtro_init(uintptr_t base)
{
    wait_idle(base);                         /* el reset deja 128 ciclos de borrado */
    for (uint32_t b = 0; b < 2; ++b) {
        HW_REG(base, F_COEF_ADDR) = b << 7;  /* COEF_DATA avanza el indice solo */
        for (uint32_t k = 0; k < FIR_MAX_TAPS; ++k)
            HW_REG(base, F_COEF_DATA) = (uint32_t)(uint16_t)(k < FIR_NTAPS ? fir_coefs[b][k] : 0);
    }
    HW_REG(base, F_NTAPS) = FIR_NTAPS;
    HW_REG(base, F_GAIN) = FILTRO_GAIN_UNITY;
    filtro_config(base, 1, FILTRO_BANK_LOWPASS);
    filtro_clear(base);
    (void)filtro_take_sat(base);
}

void filtro_config(uintptr_t base, int bypass, uint32_t bank)
{
    ctrl_shadow = (bypass ? CTRL_BYPASS : 0u) | (bank ? CTRL_BANK : 0u);
    HW_REG(base, F_CTRL) = ctrl_shadow;
}

void filtro_set_gain(uintptr_t base, uint32_t gain_q15)
{
    HW_REG(base, F_GAIN) = gain_q15 & 0xFFFFu;
}

void filtro_clear(uintptr_t base)
{
    HW_REG(base, F_CTRL) = ctrl_shadow | CTRL_CLEAR;   /* espera si estaba procesando */
    wait_idle(base);
}

void filtro_write(uintptr_t base, uint32_t lr)
{
    HW_REG(base, F_DATA_IN) = lr;
}

uint32_t filtro_read(uintptr_t base)
{
    return HW_REG(base, F_DATA_OUT);
}

int filtro_take_sat(uintptr_t base)
{
    int sat = (HW_REG(base, F_STATUS) & STATUS_SAT) != 0;
    HW_REG(base, F_STATUS) = STATUS_SAT;
    return sat;
}

/* ---- Autoprueba ----
 * El modelo completo del FIR en el Nios V cuesta ~16 000 ciclos por par (sin
 * multiplicador), asi que los casos largos usan la respuesta analitica, que se calcula
 * con una multiplicacion o solo sumas; el modelo bit a bit (dsp_fir) se usa en unos
 * pocos pares de ruido. Todo se compara con el mismo redondeo y saturacion de CD-14.
 */

static uint32_t mismatch(uintptr_t base, int16_t l, int16_t r, int16_t el, int16_t er)
{
    filtro_write(base, filtro_pack(l, r));
    return filtro_read(base) != filtro_pack(el, er);
}

static void prepare(uintptr_t base, uint32_t bank, int bypass, uint32_t gain)
{
    filtro_config(base, bypass, bank);
    filtro_set_gain(base, gain);
    filtro_clear(base);
}

/* Impulso de +32767 en L y -32768 en R: la salida es cada coeficiente escalado. */
static uint32_t test_impulse(uintptr_t base, uint32_t bank)
{
    prepare(base, bank, 0, FILTRO_GAIN_UNITY);
    uint32_t bad = 0;
    for (uint32_t k = 0; k < FIR_NTAPS; ++k) {
        int32_t h = fir_coefs[bank][k];
        int16_t el = dsp_sat16(((int64_t)h * INT16_MAX + (1 << 14)) >> 15);
        int16_t er = dsp_sat16(((int64_t)h * INT16_MIN + (1 << 14)) >> 15);
        bad += mismatch(base, k == 0 ? INT16_MAX : 0, k == 0 ? INT16_MIN : 0, el, er);
    }
    return bad;
}

/* Escalon de 0.5 (L) y -0.5 (R): la salida es media suma acumulada. */
static uint32_t test_step(uintptr_t base, uint32_t bank)
{
    prepare(base, bank, 0, FILTRO_GAIN_UNITY);
    uint32_t bad = 0;
    int32_t acc = 0;
    for (uint32_t k = 0; k < FIR_NTAPS + 8u; ++k) {
        if (k < FIR_NTAPS)
            acc += fir_coefs[bank][k];
        int16_t el = dsp_sat16(((int64_t)acc * 16384 + (1 << 14)) >> 15);
        int16_t er = dsp_sat16(((int64_t)acc * -16384 + (1 << 14)) >> 15);
        bad += mismatch(base, 16384, -16384, el, er);
    }
    return bad;
}

/* Bypass con ganancia: la salida es la entrada por la ganancia. */
static uint32_t test_bypass_gain(uintptr_t base, uint32_t gain)
{
    prepare(base, FILTRO_BANK_LOWPASS, 1, gain);
    uint32_t bad = 0;
    int16_t x = 1;
    for (uint32_t i = 0; i < 32; ++i) {
        x = (int16_t)((x << 2) + x + 12345);                /* x*5 + c sin multiplicar */
        int16_t y = (int16_t)~x;
        bad += mismatch(base, x, y, dsp_gain(x, gain, 0), dsp_gain(y, gain, 0));
    }
    return bad;
}

/* Ruido por el modelo bit a bit completo (pocos pares: es caro en el Nios V). */
static uint32_t test_model(uintptr_t base, uint32_t bank, uint32_t gain, uint32_t n)
{
    static dsp_fir_t ml, mr;                 /* static: fuera de la pila */
    dsp_fir_init(&ml, fir_coefs[bank], FIR_NTAPS);
    dsp_fir_init(&mr, fir_coefs[bank], FIR_NTAPS);
    prepare(base, bank, 0, gain);
    uint32_t bad = 0, lfsr = 0xACE1u;
    for (uint32_t i = 0; i < n; ++i) {
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);     /* LFSR de 16 bits */
        int16_t l = (int16_t)lfsr, r = (int16_t)(lfsr ^ 0x5A5Au);
        bad += mismatch(base, l, r, dsp_gain(dsp_fir_step(&ml, l, 0), gain, 0),
                        dsp_gain(dsp_fir_step(&mr, r, 0), gain, 0));
    }
    return bad;
}

uint32_t filtro_selftest(uintptr_t base)
{
    uint32_t bad = 0;
    bad += test_impulse(base, FILTRO_BANK_LOWPASS);
    bad += test_impulse(base, FILTRO_BANK_BANDPASS);
    bad += test_step(base, FILTRO_BANK_LOWPASS);
    bad += test_bypass_gain(base, 0x4000u);
    bad += test_model(base, FILTRO_BANK_BANDPASS, 0x6000u, 8);
    filtro_set_gain(base, FILTRO_GAIN_UNITY);
    filtro_config(base, 1, FILTRO_BANK_LOWPASS);
    filtro_clear(base);
    (void)filtro_take_sat(base);
    return bad;
}
