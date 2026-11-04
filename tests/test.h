/*
 * Marco minimo de pruebas en C (DK-14): sin dependencias, un solo header.
 *
 *   CHECK(cond)             falla si cond es falso (sigue con la prueba)
 *   CHECK_EQ(a, b)          compara enteros y muestra los dos valores
 *   CHECK_STR(a, b)         compara cadenas
 *   RUN(fn)                 corre una funcion de prueba y la reporta
 *   return TEST_REPORT();   al final de main: imprime el resumen, 0 si todo paso
 */
#ifndef TESTS_TEST_H
#define TESTS_TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures;
static int test_checks;
static int test_case_failures;

#define CHECK(cond) do { \
    test_checks++; \
    if (!(cond)) { \
        printf("  FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        test_failures++; \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    long long va_ = (long long)(a), vb_ = (long long)(b); \
    test_checks++; \
    if (va_ != vb_) { \
        printf("  FALLO %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, \
               va_, vb_); \
        test_failures++; \
    } \
} while (0)

#define CHECK_STR(a, b) do { \
    const char *sa_ = (a), *sb_ = (b); \
    test_checks++; \
    if (strcmp(sa_, sb_) != 0) { \
        printf("  FALLO %s:%d: %s == \"%s\" (es \"%s\")\n", __FILE__, __LINE__, #a, sb_, sa_); \
        test_failures++; \
    } \
} while (0)

#define RUN(fn) do { \
    int before_ = test_failures; \
    fn(); \
    if (test_failures != before_) test_case_failures++; \
    printf("%-6s %s\n", test_failures == before_ ? "OK" : "FALLO", #fn); \
} while (0)

#define TEST_REPORT() ( \
    printf("%s: %d comprobaciones, %d fallos\n", test_failures ? "FALLO" : "PASA", \
           test_checks, test_failures), \
    test_failures ? 1 : 0)

#endif /* TESTS_TEST_H */
