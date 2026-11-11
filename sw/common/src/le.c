#include "le.h"

/* off + n podria desbordar si off es enorme: se compara contra len - n. */
static int fits(size_t len, size_t off, size_t n)
{
    return len >= n && off <= len - n;
}

err_t le_read16(const uint8_t *buf, size_t len, size_t off, uint16_t *out)
{
    if (!buf || !out)
        return ERR_ARG;
    if (!fits(len, off, 2))
        return ERR_BOUNDS;
    *out = le16(buf + off);
    return ERR_OK;
}

err_t le_read32(const uint8_t *buf, size_t len, size_t off, uint32_t *out)
{
    if (!buf || !out)
        return ERR_ARG;
    if (!fits(len, off, 4))
        return ERR_BOUNDS;
    *out = le32(buf + off);
    return ERR_OK;
}

int le_fourcc_eq(const uint8_t *p, const char *id)
{
    return p[0] == (uint8_t)id[0] && p[1] == (uint8_t)id[1] && p[2] == (uint8_t)id[2] &&
           p[3] == (uint8_t)id[3];
}
