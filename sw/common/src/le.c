/* Lectura little-endian con comprobacion de limites (SC-02). Ver le.h. */
#include "le.h"

/*
 * 1 si el rango [off, off + n) cabe dentro de un buffer de len bytes.
 * off + n podria desbordar si off es enorme: se compara contra len - n.
 */
static int fits(size_t len, size_t off, size_t n)
{
    return len >= n && off <= len - n;
}

/*
 * Lee un entero de 16 bits little-endian en buf[off]. Devuelve ERR_ARG si algun
 * puntero es nulo, ERR_BOUNDS si los 2 bytes no caben en len, o ERR_OK con el valor
 * en *out.
 */
err_t le_read16(const uint8_t *buf, size_t len, size_t off, uint16_t *out)
{
    if (!buf || !out)
        return ERR_ARG;
    if (!fits(len, off, 2))
        return ERR_BOUNDS;
    *out = le16(buf + off);
    return ERR_OK;
}

/* Igual que le_read16 pero para un entero de 32 bits (4 bytes). */
err_t le_read32(const uint8_t *buf, size_t len, size_t off, uint32_t *out)
{
    if (!buf || !out)
        return ERR_ARG;
    if (!fits(len, off, 4))
        return ERR_BOUNDS;
    *out = le32(buf + off);
    return ERR_OK;
}

/*
 * 1 si los 4 bytes de p son exactamente los 4 caracteres de id (por ejemplo "RIFF",
 * "fmt ", "data"). No mira el '\0' de id ni necesita que p termine en '\0'.
 */
int le_fourcc_eq(const uint8_t *p, const char *id)
{
    return p[0] == (uint8_t)id[0] && p[1] == (uint8_t)id[1] && p[2] == (uint8_t)id[2] &&
           p[3] == (uint8_t)id[3];
}
