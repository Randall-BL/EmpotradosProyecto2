#include "err.h"

const char *err_str(err_t e)
{
    switch (e) {
    case ERR_OK:          return "OK";
    case ERR_ARG:         return "ARGUMENTO";
    case ERR_BOUNDS:      return "FUERA DE RANGO";
    case ERR_IO:          return "LECTURA";
    case ERR_FORMAT:      return "FORMATO";
    case ERR_UNSUPPORTED: return "NO SOPORTADO";
    case ERR_TRUNCATED:   return "TRUNCADO";
    case ERR_FULL:        return "LLENO";
    case ERR_EMPTY:       return "VACIO";
    case ERR_VERSION:     return "VERSION";
    }
    return "DESCONOCIDO";
}
