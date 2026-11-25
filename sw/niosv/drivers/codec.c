/*
 * Driver del IP Audio and Video Config para el codec WM8731 (NV-07). Ver codec.h.
 *
 * El IP habla I2C con el codec. El software solo pone dispositivo, registro y dato;
 * escribir el dato arranca la transferencia, y el registro de estado avisa cuando
 * termino (RDY) y si el codec respondio con error (ACK).
 */
#include "codec.h"
#include "hw.h"

/* Desplazamientos de los registros del IP de configuracion. */
#define CFG_REG_CONTROL 0x0   /* dispositivo destino y reinicio */
#define CFG_REG_STATUS  0x4   /* ACK, RDY y AIS */
#define CFG_REG_ADDRESS 0x8   /* registro del dispositivo */
#define CFG_REG_DATA    0xC   /* dato; escribirlo inicia la transferencia */

/* Bits del registro de estado. */
#define CFG_STATUS_ACK (1u << 0)   /* 1 = el codec no reconocio la transferencia */
#define CFG_STATUS_RDY (1u << 1)   /* 1 = listo para otra transferencia */
#define CFG_STATUS_AIS (1u << 8)   /* 1 = autoinicializacion completa */

#define CFG_DEV_AUDIO  (0u << 16)  /* campo DEV = 0: el codec de audio */

/* Sondea el registro de estado hasta que alguno de los bits de mask valga 1.
 * Devuelve CODEC_ERR_TIMEOUT si se agotan las timeout lecturas. */
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

/*
 * Escribe value (9 bits) en el registro reg (7 bits) del WM8731. Espera a que el IP
 * este libre, arranca la transferencia, confirma que arranco y espera el final.
 * Devuelve CODEC_OK, CODEC_ERR_TIMEOUT (el IP no respondio) o CODEC_ERR_NACK (el codec
 * rechazo la escritura).
 */
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
        /* RDY en 0 = la transferencia arranco. */
        for (uint32_t i = 0; i < START_POLLS; ++i) {
            if ((HW_REG(base, CFG_REG_STATUS) & CFG_STATUS_RDY) == 0) {
                started = 1;
                break;
            }
        }
    }
    if (!started)
        return CODEC_ERR_TIMEOUT;

    /* RDY vuelve a 1 al terminar; ahi ACK dice si el codec acepto el dato. */
    rc = wait_status(base, CFG_STATUS_RDY, timeout);
    if (rc != CODEC_OK)
        return rc;
    return (HW_REG(base, CFG_REG_STATUS) & CFG_STATUS_ACK) ? CODEC_ERR_NACK : CODEC_OK;
}

/* Espera a que el IP termine de configurar el codec al arrancar y despues ajusta el
 * registro R5 (control digital) para quitar la de-enfasis. */
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

/* Silencia (mute != 0) o reactiva la salida del DAC con el bit DACMU de R5. */
int codec_mute(uintptr_t base, int mute, uint32_t timeout)
{
    return codec_write(base, WM8731_R5_DIGITAL, mute ? WM8731_R5_DACMU : 0x000, timeout);
}
