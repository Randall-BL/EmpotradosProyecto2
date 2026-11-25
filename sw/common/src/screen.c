/*
 * Pantalla del reproductor en modo texto 80 columnas (NV-11). Ver screen.h para la
 * distribucion de las filas.
 *
 * Todo se dibuja con el puerto screen_port_t::print(x, y, texto, ancho), que escribe
 * el texto y rellena con espacios hasta "ancho" celdas (0 = sin relleno). Los numeros
 * se formatean con restas (sin printf ni division).
 */
#include "screen.h"

#define LAST_COL (SCREEN_COLS - 1u)    /* columna del borde derecho */
#define INNER    (SCREEN_COLS - 4u)    /* ancho util entre "| " y " |" */
#define BAR_LEN  20u                   /* casillas de la barra de volumen (5 % cada una) */

/*
 * Dibuja una linea horizontal completa en la fila y: el caracter end en los dos
 * extremos y fill en el medio. Ej.: "+----...----+" o "|----...----|".
 */
static void hline(const screen_port_t *p, uint32_t y, char end, char fill)
{
    char line[SCREEN_COLS + 1];
    line[0] = end;
    for (uint32_t i = 1; i < LAST_COL; ++i)
        line[i] = fill;
    line[LAST_COL] = end;
    line[SCREEN_COLS] = '\0';
    p->print(p->ctx, 0, y, line, 0);
}

/* Fila con bordes: "| " + texto + relleno + " |". */
static void row(const screen_port_t *p, uint32_t y, const char *s)
{
    p->print(p->ctx, 0, y, "| ", 0);
    p->print(p->ctx, 2, y, s, INNER);
    p->print(p->ctx, LAST_COL - 1u, y, " |", 0);
}

/* Copia como mucho max caracteres de src en dst (terminado en '\0'). Un src nulo se
 * trata como cadena vacia. Devuelve la cantidad de caracteres copiados. */
static uint32_t copy(char *dst, const char *src, uint32_t max)
{
    uint32_t n = 0;
    if (src)
        for (; src[n] != '\0' && n < max; ++n)
            dst[n] = src[n];
    dst[n] = '\0';
    return n;
}

/*
 * Escribe v (saturado a 999) en decimal en out, alineado a la derecha en width
 * celdas y rellenando a la izquierda con pad (' ' o '0'). Si el numero tiene mas
 * cifras que width se escribe completo. out necesita max(width, 3) + 1 bytes.
 */
void screen_fmt_uint(char *out, uint32_t v, uint32_t width, char pad)
{
    if (v > 999u)
        v = 999u;
    /* Centenas, decenas y unidades por restas sucesivas. */
    char d[3];
    uint32_t c = 0, t = 0;
    while (v >= 100u) { v -= 100u; ++c; }
    while (v >= 10u)  { v -= 10u;  ++t; }
    d[0] = (char)('0' + c);
    d[1] = (char)('0' + t);
    d[2] = (char)('0' + v);
    uint32_t first = (c != 0) ? 0u : (t != 0) ? 1u : 2u;   /* primer digito significativo */
    uint32_t digits = 3u - first;
    if (width < digits)
        width = digits;
    /* Relleno a la izquierda y despues los digitos significativos. */
    uint32_t i = 0;
    for (; i + digits < width; ++i)
        out[i] = pad;
    for (uint32_t k = first; k < 3u; ++k)
        out[i++] = d[k];
    out[i] = '\0';
}

/* Formatea seconds como "MM:SS" (con ceros a la izquierda); satura en "99:59". */
void screen_fmt_mmss(char *out, uint32_t seconds)
{
    uint32_t m = 0;
    if (seconds > 99u * 60u + 59u)
        seconds = 99u * 60u + 59u;
    while (seconds >= 60u) { seconds -= 60u; ++m; }
    screen_fmt_uint(out, m, 2, '0');
    out[2] = ':';
    screen_fmt_uint(out + 3, seconds, 2, '0');
}

/*
 * Dibuja la parte fija de la pantalla: marco, titulo de la aplicacion, filas vacias
 * del cuerpo (3 a 9) y la ayuda de los botones. Se llama una sola vez al arrancar.
 */
void screen_draw_frame(const screen_port_t *p)
{
    hline(p, 0, '+', '-');
    row(p, 1, "REPRODUCTOR DE AUDIO - CE1113");
    hline(p, 2, '|', '-');
    for (uint32_t y = 3; y <= 9; ++y)
        row(p, y, "");
    hline(p, 10, '|', '-');
    row(p, 11, "KEY0 Play/Pausa  KEY1 Stop  KEY2 Anterior  KEY3 Siguiente   SW: filtro");
    hline(p, 12, '+', '-');
}

/* Fila "etiqueta + valor" (ej. "Titulo  : Mi cancion"); el valor se corta en
 * SCREEN_FIELD_MAX caracteres para que no pise el borde. */
static void labeled(const screen_port_t *p, uint32_t y, const char *label, const char *value)
{
    char buf[SCREEN_COLS];
    uint32_t n = copy(buf, label, 10);
    copy(buf + n, value, SCREEN_FIELD_MAX);
    row(p, y, buf);
}

/*
 * Dibuja los datos de la cancion: "Cancion n / total" arriba a la derecha, titulo,
 * artista, album y duracion total. Se llama al cambiar de cancion.
 */
void screen_draw_track(const screen_port_t *p, const screen_track_t *t)
{
    char num[8];
    char buf[24];

    /* "Cancion  NN / NN" alineado a la derecha de la fila 1. */
    uint32_t n = copy(buf, "Cancion ", 8);
    screen_fmt_uint(num, t->track, 3, ' ');
    n += copy(buf + n, num, 3);
    n += copy(buf + n, " / ", 3);
    screen_fmt_uint(num, t->total, 2, '0');
    copy(buf + n, num, 3);
    p->print(p->ctx, 60, 1, buf, 18);

    labeled(p, 3, "Titulo  : ", t->title);
    labeled(p, 4, "Artista : ", t->artist);
    labeled(p, 5, "Album   : ", t->album);

    /* Duracion en la mitad izquierda de la fila 6 (la derecha es el transcurrido). */
    char mmss[6];
    screen_fmt_mmss(mmss, t->duration_s);
    p->print(p->ctx, 2, 6, "Duracion: ", 0);
    p->print(p->ctx, 12, 6, mmss, 5);
}

/*
 * Dibuja la zona que cambia durante la reproduccion:
 *   fila 6 (derecha): tiempo transcurrido;
 *   fila 8: estado (con un simbolo tipo [] > ||) y nombre del filtro;
 *   fila 9: barra de volumen de 20 casillas y porcentaje.
 */
void screen_draw_status(const screen_port_t *p, screen_state_t st, const char *filter,
                        uint32_t volume_percent, uint32_t elapsed_s)
{
    static const char *const names[] = { "[] DETENIDO", ">  REPRODUCIENDO", "|| PAUSADO" };
    char buf[SCREEN_COLS];

    char mmss[6];
    screen_fmt_mmss(mmss, elapsed_s);
    p->print(p->ctx, 35, 6, "Transcurrido: ", 0);
    p->print(p->ctx, 49, 6, mmss, 5);

    /* Un estado fuera de rango se muestra como DETENIDO en vez de leer fuera de la
     * tabla. */
    p->print(p->ctx, 2, 8, "Estado  : ", 0);
    p->print(p->ctx, 12, 8, names[st <= SCREEN_PAUSED ? st : 0], 20);
    uint32_t n = copy(buf, "Filtro: ", 8);
    copy(buf + n, filter, 30);
    p->print(p->ctx, 35, 8, buf, 40);

    /* Casillas llenas = volume_percent / 5, contadas con restas. */
    if (volume_percent > 100u)
        volume_percent = 100u;
    uint32_t fill = 0;
    for (uint32_t v = volume_percent; v >= 5u; v -= 5u)   /* 20 casillas de 5 % */
        ++fill;
    /* "Volumen : [#####---------------] NNN%" */
    n = copy(buf, "Volumen : [", 11);
    for (uint32_t i = 0; i < BAR_LEN; ++i)
        buf[n++] = (i < fill) ? '#' : '-';
    buf[n++] = ']';
    buf[n++] = ' ';
    screen_fmt_uint(buf + n, volume_percent, 3, ' ');
    n += 3;
    buf[n++] = '%';
    buf[n] = '\0';
    p->print(p->ctx, 2, 9, buf, 40);
}

/* Escribe msg en la linea de mensajes (fila 7), borrando lo que hubiera. NULL o ""
 * dejan la linea en blanco. */
void screen_draw_message(const screen_port_t *p, const char *msg)
{
    p->print(p->ctx, 2, 7, msg ? msg : "", INNER);
}
