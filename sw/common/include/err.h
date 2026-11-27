/*
 * Codigos de error comunes de sw/common (SC-01).
 *
 * Todas las funciones que pueden fallar devuelven err_t: ERR_OK (0) o un valor
 * negativo. El resultado util se entrega por puntero. Asi el Nios V, el HPS y las
 * pruebas del PC reportan los errores igual.
 */
#ifndef COMMON_ERR_H
#define COMMON_ERR_H

typedef enum {
    ERR_OK          =  0,
    ERR_ARG         = -1,   /* argumento invalido (puntero nulo, tamano 0) */
    ERR_BOUNDS      = -2,   /* lectura fuera del buffer o del archivo */
    ERR_IO          = -3,   /* el lector no pudo entregar los bytes */
    ERR_FORMAT      = -4,   /* no es RIFF/WAVE o un chunk esta mal formado */
    ERR_UNSUPPORTED = -5,   /* WAV valido pero formato no soportado */
    ERR_TRUNCATED   = -6,   /* el archivo termina antes de lo que declara */
    ERR_FULL        = -7,   /* cola o buffer lleno */
    ERR_EMPTY       = -8,   /* cola, buffer o lista vacia */
    ERR_VERSION     = -9    /* version del protocolo distinta */
} err_t;

/* Texto corto (<= 16 caracteres) del error, para la pantalla o la UART. */
const char *err_str(err_t e);

#endif /* COMMON_ERR_H */
