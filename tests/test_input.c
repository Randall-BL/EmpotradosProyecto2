/*
 * Pruebas de la cola de eventos (SC-09), los switches (CD-08), el antirrebote (NV-04)
 * y el acumulador de milisegundos (NV-12).
 */
#include "clock.h"
#include "debounce.h"
#include "evq.h"
#include "switches.h"
#include "test.h"

static void test_evq(void)
{
    evq_t q;
    uint8_t ev = 0xAA;
    evq_init(&q);
    CHECK_EQ(evq_pop(&q, &ev), ERR_EMPTY);
    CHECK_EQ(ev, 0xAA);
    for (uint8_t i = 0; i < EVQ_SIZE; ++i)
        CHECK_EQ(evq_push(&q, i), ERR_OK);
    CHECK_EQ(evq_count(&q), EVQ_SIZE);
    CHECK_EQ(evq_push(&q, 99), ERR_FULL);      /* llena: se descarta y se cuenta */
    CHECK_EQ(q.dropped, 1);
    for (uint8_t i = 0; i < EVQ_SIZE; ++i) {
        CHECK_EQ(evq_pop(&q, &ev), ERR_OK);
        CHECK_EQ(ev, i);                       /* orden FIFO */
    }
    CHECK_EQ(evq_count(&q), 0);
}

static void test_evq_wrap(void)
{
    /* Los indices corren libres: deben funcionar al dar la vuelta de 32 bits. */
    evq_t q;
    evq_init(&q);
    q.head = q.tail = 0xFFFFFFF0u;
    uint8_t ev;
    for (uint32_t round = 0; round < 100; ++round) {
        for (uint8_t i = 0; i < 5; ++i)
            CHECK_EQ(evq_push(&q, (uint8_t)(round + i)), ERR_OK);
        for (uint8_t i = 0; i < 5; ++i) {
            CHECK_EQ(evq_pop(&q, &ev), ERR_OK);
            CHECK_EQ(ev, (uint8_t)(round + i));
        }
    }
    CHECK(q.head < 0x1000u);                   /* dio la vuelta */
    CHECK_EQ(evq_count(&q), 0);
}

static void test_switch_decode(void)
{
    int bad = -1;
    CHECK_EQ(sw_decode(0x0, &bad), FILTER_BYPASS);   CHECK_EQ(bad, 0);
    CHECK_EQ(sw_decode(0x1, &bad), FILTER_LOWPASS);  CHECK_EQ(bad, 0);
    CHECK_EQ(sw_decode(0x2, &bad), FILTER_BANDPASS); CHECK_EQ(bad, 0);
    CHECK_EQ(sw_decode(0x4, &bad), FILTER_REVERB);   CHECK_EQ(bad, 0);
    CHECK_EQ(sw_decode(0x3F8, &bad), FILTER_BYPASS); CHECK_EQ(bad, 0);  /* SW[9:3] no cuentan */
    CHECK_EQ(sw_decode(0x3F9, &bad), FILTER_LOWPASS);
    for (uint32_t v = 0; v < 8; ++v) {
        int n = (v & 1) + ((v >> 1) & 1) + ((v >> 2) & 1);
        sw_decode(v, &bad);
        CHECK_EQ(bad, n > 1);
        if (n > 1)
            CHECK_EQ(sw_decode(v, NULL), FILTER_BYPASS);
    }
    CHECK_STR(sw_filter_name(FILTER_REVERB), "REVERBERACION 50 ms");
    CHECK_STR(sw_filter_name(FILTER_COUNT), "?");
}

static void test_switch_update(void)
{
    sw_state_t s;
    sw_init(&s, 0x1);
    CHECK_EQ(s.filter, FILTER_LOWPASS);
    CHECK_EQ(sw_update(&s, 0x1), 0);
    /* Un cambio se acepta cuando la lectura se repite (20 ms despues). */
    CHECK_EQ(sw_update(&s, 0x2), 0);
    CHECK_EQ(s.filter, FILTER_LOWPASS);
    CHECK_EQ(sw_update(&s, 0x2), 1);
    CHECK_EQ(s.filter, FILTER_BANDPASS);
    /* Un rebote de una sola lectura no cambia nada. */
    CHECK_EQ(sw_update(&s, 0x0), 0);
    CHECK_EQ(sw_update(&s, 0x2), 0);
    CHECK_EQ(s.filter, FILTER_BANDPASS);
    /* Combinacion invalida: bypass y marca. */
    sw_update(&s, 0x6);
    CHECK_EQ(sw_update(&s, 0x6), 1);
    CHECK_EQ(s.filter, FILTER_BYPASS);
    CHECK_EQ(s.invalid, 1);
    /* De invalida (bypass) a ninguno (bypass): cambia la marca, no el filtro. */
    sw_update(&s, 0x0);
    CHECK_EQ(sw_update(&s, 0x0), 1);
    CHECK_EQ(s.invalid, 0);
    CHECK_EQ(s.filter, FILTER_BYPASS);
}

static void test_debounce(void)
{
    deb_t d;
    deb_init(&d);
    CHECK(deb_accept(&d, 0, 0));               /* primer flanco en t = 0 */
    CHECK(!deb_accept(&d, 0, 10));             /* rebote */
    CHECK(!deb_accept(&d, 0, DEB_WINDOW_MS - 1));
    CHECK(deb_accept(&d, 1, 10));              /* otro boton: independiente */
    CHECK(deb_accept(&d, 0, DEB_WINDOW_MS));   /* fuera de la ventana */
    CHECK_EQ(d.rejected, 2);
    CHECK(!deb_accept(&d, 4, 1000));           /* boton inexistente */
    /* Vuelta del contador de ms. */
    CHECK(deb_accept(&d, 2, 0xFFFFFFF0u));
    CHECK(!deb_accept(&d, 2, 0x10u));          /* 32 ms despues */
    CHECK(deb_accept(&d, 2, 0x40u));           /* 80 ms despues */
}

static void test_clock(void)
{
    clk_t c;
    clk_init(&c, 50000u, 1000u);               /* 50 MHz */
    CHECK_EQ(clk_update(&c, 1000u), 0);
    CHECK_EQ(clk_update(&c, 1000u + 49999u), 0);
    CHECK_EQ(clk_update(&c, 1000u + 50000u), 1);
    CHECK_EQ(clk_update(&c, 1000u + 50000u * 1234u + 7u), 1234);
    /* Vuelta del contador de 32 bits. */
    clk_init(&c, 50000u, 0xFFFFFF00u);
    CHECK_EQ(clk_update(&c, 0xFFFFFF00u + 50000u * 3u), 3);
    /* Muchas llamadas pequenas suman lo mismo que una grande (sin perder fraccion). */
    clk_init(&c, 50000u, 0);
    uint32_t t = 0;
    for (int i = 0; i < 100000; ++i) {
        t += 1237u;
        clk_update(&c, t);
    }
    CHECK_EQ(c.ms, (uint32_t)(((unsigned long long)t) / 50000u));
}

int main(void)
{
    RUN(test_evq);
    RUN(test_evq_wrap);
    RUN(test_switch_decode);
    RUN(test_switch_update);
    RUN(test_debounce);
    RUN(test_clock);
    return TEST_REPORT();
}
