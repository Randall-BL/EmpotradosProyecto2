/*
 * Lectura little-endian segura (SC-02).
 *
 * Los formatos que se leen (RIFF/WAVE, el protocolo de memoria compartida) son
 * little-endian. Se arma cada valor byte a byte: no depende del orden de bytes del
 * procesador ni de que la direccion este alineada (el Nios V/m falla con accesos
 * desalineados). Cada lectura comprueba que off + ancho quepa en len.
 */
#ifndef COMMON_LE_H
#define COMMON_LE_H

#include <stddef.h>
#include <stdint.h>
#include "err.h"

/* Sin comprobacion: el llamador ya sabe que hay 2 o 4 bytes en p. */
static inline uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Con comprobacion de limites: ERR_BOUNDS si [off, off+ancho) no cabe en len. */
err_t le_read16(const uint8_t *buf, size_t len, size_t off, uint16_t *out);
err_t le_read32(const uint8_t *buf, size_t len, size_t off, uint32_t *out);

/* Compara 4 bytes con un identificador de chunk ("RIFF", "fmt "...). */
int le_fourcc_eq(const uint8_t *p, const char *id);

#endif /* COMMON_LE_H */
