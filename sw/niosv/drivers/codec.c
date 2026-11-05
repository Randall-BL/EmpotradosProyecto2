#include "codec.h"
#include "hw.h"

#define CFG_REG_CONTROL 0x0
#define CFG_REG_STATUS  0x4
#define CFG_REG_ADDRESS 0x8
#define CFG_REG_DATA    0xC

#define CFG_STATUS_ACK (1u << 0)
#define CFG_STATUS_RDY (1u << 1)
#define CFG_STATUS_AIS (1u << 8)

#define CFG_DEV_AUDIO  (0u << 16)

static int wait_status(uintptr_t base, uint32_t mask, uint32_t timeout)
{
    while ((HW_REG(base, CFG_REG_STATUS) & mask) == 0) {
        if (timeout-- == 0)
            return CODEC_ERR_TIMEOUT;
    }
    return CODEC_OK;
}

int codec_write(uintptr_t base, uint32_t reg, uint32_t value, uint32_t timeout)
{
    int rc = wait_status(base, CFG_STATUS_RDY, timeout);
    if (rc != CODEC_OK)
        return rc;

    /* Pasos de la seccion 4.2 de la guia del IP: dispositivo, registro y dato;
     * la escritura en data inicia la transferencia I2C. */
    HW_REG(base, CFG_REG_CONTROL) = CFG_DEV_AUDIO;
    HW_REG(base, CFG_REG_ADDRESS) = reg & 0x7Fu;
    HW_REG(base, CFG_REG_DATA)    = value & 0x1FFu;

    rc = wait_status(base, CFG_STATUS_RDY, timeout);
    if (rc != CODEC_OK)
        return rc;
    return (HW_REG(base, CFG_REG_STATUS) & CFG_STATUS_ACK) ? CODEC_ERR_NACK : CODEC_OK;
}

int codec_init(uintptr_t base, uint32_t timeout)
{
    int rc = wait_status(base, CFG_STATUS_AIS, timeout);
    if (rc != CODEC_OK)
        return rc;

    /* La ROM de autoinicializacion del IP deja R5 = 0x006: de-enfasis de 48 kHz,
     * que atenua los agudos (pensada para audio grabado con pre-enfasis). Se
     * apaga para que la salida sea plana y los filtros se oigan tal cual. */
    return codec_write(base, WM8731_R5_DIGITAL, 0x000, timeout);
}

int codec_mute(uintptr_t base, int mute, uint32_t timeout)
{
    return codec_write(base, WM8731_R5_DIGITAL, mute ? WM8731_R5_DACMU : 0x000, timeout);
}
