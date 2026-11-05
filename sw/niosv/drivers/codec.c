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

/* Lecturas del estado para ver arrancar una transferencia: el IP sube
 * start_external_transfer 2 ciclos despues de escribir el dato (RDY baja ahi). */
#define START_POLLS   64u
#define START_RETRIES 4u

int codec_write(uintptr_t base, uint32_t reg, uint32_t value, uint32_t timeout)
{
    int rc = wait_status(base, CFG_STATUS_RDY, timeout);
    if (rc != CODEC_OK)
        return rc;

    /* Pasos de la seccion 4.2 de la guia del IP: dispositivo, registro y dato;
     * la escritura en data inicia la transferencia I2C. Dos detalles del RTL del IP
     * (altera_up_avalon_audio_and_video_config) que este codigo cubre por seguridad:
     *  - RDY sigue en 1 durante 2 ciclos despues de escribir el dato, asi que no se
     *    da por terminada la transferencia sin haber visto bajar RDY primero.
     *  - Justo al terminar una transferencia, transfer_complete sigue en 1 un ciclo
     *    y el IP ignoraria una escritura nueva; si RDY no baja, se reintenta. */
    HW_REG(base, CFG_REG_CONTROL) = CFG_DEV_AUDIO;
    HW_REG(base, CFG_REG_ADDRESS) = reg & 0x7Fu;
    uint32_t started = 0;
    for (uint32_t attempt = 0; attempt < START_RETRIES && !started; ++attempt) {
        HW_REG(base, CFG_REG_DATA) = value & 0x1FFu;
        for (uint32_t i = 0; i < START_POLLS; ++i) {
            if ((HW_REG(base, CFG_REG_STATUS) & CFG_STATUS_RDY) == 0) {
                started = 1;
                break;
            }
        }
    }
    if (!started)
        return CODEC_ERR_TIMEOUT;

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
