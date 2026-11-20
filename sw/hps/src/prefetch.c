#include "prefetch.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)ts.tv_sec * 1000u + (uint32_t)(ts.tv_nsec / 1000000L);
}

static size_t min_sz(size_t a, size_t b)
{
    return a < b ? a : b;
}

static void *reader(void *arg)
{
    prefetch_t *p = arg;
    pthread_mutex_lock(&p->mu);
    while (!p->cancel && p->left > 0) {
        size_t space = PREFETCH_BYTES - (p->head - p->tail);
        if (space < PREFETCH_CHUNK && space < p->left) {
            pthread_cond_wait(&p->cv, &p->mu);    /* buffer lleno: esperar consumo */
            continue;
        }
        size_t pos = p->head % PREFETCH_BYTES;
        size_t n = min_sz(min_sz(space, PREFETCH_CHUNK), min_sz(PREFETCH_BYTES - pos, p->left));
        pthread_mutex_unlock(&p->mu);

        /* El consumidor no toca [head, head + n): se escribe sin el mutex. */
        uint32_t t0 = now_ms();
        size_t got = fread(p->buf + pos, 1, n, p->f);
        uint32_t ms = now_ms() - t0;

        pthread_mutex_lock(&p->mu);
        p->reads++;
        if (ms > p->max_read_ms)
            p->max_read_ms = ms;
        if (ms > PREFETCH_SLOW_MS) {
            p->slow_reads++;
            if (p->verbose)
                printf("hps_player: lectura lenta de la SD: %u ms (pista %u, %u KB, "
                       "buffer %u KB)\n", ms, p->track + 1u, (unsigned)(n >> 10),
                       (unsigned)((p->head - p->tail) >> 10));
        }
        p->head += got;
        p->left = got < n ? 0 : p->left - got;   /* corto: EOF o error de lectura */
        pthread_cond_broadcast(&p->cv);
    }
    p->done = 1;
    pthread_cond_broadcast(&p->cv);
    pthread_mutex_unlock(&p->mu);
    return NULL;
}

int prefetch_start(prefetch_t *p, FILE *f, size_t bytes, uint32_t track)
{
    prefetch_stop(p);
    if (!p->ready) {
        if (pthread_mutex_init(&p->mu, NULL) != 0 || pthread_cond_init(&p->cv, NULL) != 0)
            return -1;
        p->ready = 1;
    }
    if (!p->buf && !(p->buf = malloc(PREFETCH_BYTES)))
        return -1;
    p->f = f;
    p->left = bytes;
    p->head = p->tail = 0;
    p->done = p->cancel = 0;
    p->track = track;
    p->reads = p->slow_reads = p->max_read_ms = 0;
    p->min_level = (size_t)-1;
    if (pthread_create(&p->th, NULL, reader, p) != 0) {
        p->f = NULL;
        return -1;
    }
    p->running = 1;
    return 0;
}

void prefetch_wait(prefetch_t *p, size_t min)
{
    if (!p->running)
        return;
    pthread_mutex_lock(&p->mu);
    while (p->head - p->tail < min && !p->done)
        pthread_cond_wait(&p->cv, &p->mu);
    pthread_mutex_unlock(&p->mu);
}

size_t prefetch_read(prefetch_t *p, void *dst, size_t max, size_t align)
{
    if (!p->running)
        return 0;
    pthread_mutex_lock(&p->mu);
    size_t avail = p->head - p->tail;
    if (avail < p->min_level && !p->done)
        p->min_level = avail;
    size_t n = min_sz(avail, max);
    n -= n % align;
    size_t pos = p->tail % PREFETCH_BYTES;
    size_t first = min_sz(n, PREFETCH_BYTES - pos);
    memcpy(dst, p->buf + pos, first);
    memcpy((uint8_t *)dst + first, p->buf, n - first);
    p->tail += n;
    if (n)
        pthread_cond_broadcast(&p->cv);
    pthread_mutex_unlock(&p->mu);
    return n;
}

int prefetch_finished(prefetch_t *p, size_t align)
{
    if (!p->running)
        return 1;
    pthread_mutex_lock(&p->mu);
    int fin = p->done && p->head - p->tail < align;
    pthread_mutex_unlock(&p->mu);
    return fin;
}

size_t prefetch_level(prefetch_t *p)
{
    if (!p->running)
        return 0;
    pthread_mutex_lock(&p->mu);
    size_t n = p->head - p->tail;
    pthread_mutex_unlock(&p->mu);
    return n;
}

void prefetch_stop(prefetch_t *p)
{
    if (p->running) {
        pthread_mutex_lock(&p->mu);
        p->cancel = 1;
        pthread_cond_broadcast(&p->cv);
        pthread_mutex_unlock(&p->mu);
        pthread_join(p->th, NULL);
        p->running = 0;
    }
    if (p->f)
        fclose(p->f);
    p->f = NULL;
}
