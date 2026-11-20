/*
 * Aplicacion del HPS (HP-05..HP-09): la lanza el init de Linux al arrancar, sin consola
 * de usuario (HP-08). Recorre la carpeta de canciones de la SD, publica la tabla en shm
 * y entrega el PCM de la pista que pida el Nios V (producer.c).
 *
 * La FPGA puede configurarse despues de Linux (JTAG durante el desarrollo) o
 * reconfigurarse mientras corre: antes de tocar el puente se revisa que este en modo
 * usuario, y si shm pierde el "magic" (RAM de la FPGA reiniciada) se vuelve a publicar.
 *
 * Tiempo real (HP-07): el lazo corre con SCHED_FIFO y la memoria bloqueada (mlockall),
 * y la SD la lee un hilo aparte (prefetch.c). En la consola solo se reportan anomalias:
 * lecturas lentas, ring vacio, pausas del lazo y un resumen al terminar cada pista.
 *
 * Uso: hps_player [carpeta]   (por defecto /mnt/sd/music)
 */
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "devmem.h"
#include "producer.h"

#define DEFAULT_DIR "/mnt/sd/music"
#define RT_PRIORITY 50                    /* SCHED_FIFO, por encima de lo normal */
#define LOOP_SLOW_MS 100u                 /* pausa del lazo que se reporta */

static producer_t prod;
static volatile sig_atomic_t quit;

static void on_signal(int sig)
{
    (void)sig;
    quit = 1;
}

static void sleep_us(long us)
{
    struct timespec ts = {us / 1000000L, (us % 1000000L) * 1000L};
    nanosleep(&ts, NULL);
}

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)ts.tv_sec * 1000u + (uint32_t)(ts.tv_nsec / 1000000L);
}

/* Prioridad de tiempo real y memoria sin paginar. Si falla se sigue igual. */
static void realtime_setup(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("hps_player: mlockall");
    struct sched_param sp;
    memset(&sp, 0, sizeof sp);
    sp.sched_priority = RT_PRIORITY;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        perror("hps_player: SCHED_FIFO");
    else
        printf("hps_player: SCHED_FIFO %d, memoria bloqueada\n", RT_PRIORITY);
}

static const char *state_name(uint32_t st)
{
    switch (st) {
    case SHM_HPS_READY:     return "lista";
    case SHM_HPS_NO_SD:     return "sin SD o sin carpeta";
    case SHM_HPS_NO_TRACKS: return "sin WAV validos";
    default:                return "?";
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : DEFAULT_DIR;
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    uint32_t n = producer_scan(&prod, dir);
    printf("hps_player: %s: %u pistas, %u descartadas (%s)\n", dir, n, prod.skipped,
           state_name(prod.hps_state));
    for (uint32_t i = 0; i < n; ++i)
        printf("  %02u %s - %s (%u Hz, %u ch, %u s)\n", i + 1, prod.table[i].title,
               prod.table[i].artist, prod.table[i].sample_rate, prod.table[i].channels,
               prod.table[i].duration_s);

    prod.verbose = 1;
    realtime_setup();                     /* el hilo lector hereda la politica */

    devmem_t d = {.fd = -1};
    if (devmem_open(&d) != 0) {
        perror("hps_player: /dev/mem");
        return 1;
    }

    int online = 0;
    uint32_t last = now_ms();
    while (!quit) {
        uint32_t t = now_ms();
        if (prod.active && t - last > LOOP_SLOW_MS)
            printf("hps_player: lazo detenido %u ms (pista %u)\n", t - last, prod.track + 1u);
        last = t;
        if (!devmem_fpga_ready(&d)) {
            if (online)
                printf("hps_player: la FPGA salio de modo usuario; esperando\n");
            online = 0;
            producer_stop(&prod);
            sleep_us(100000);
            continue;
        }
        volatile shm_t *s = d.shm;
        if (!online || s->ctrl.magic != SHM_MAGIC) {
            devmem_lwbridge_enable(&d);
            producer_publish(&prod, s);
            online = 1;
            printf("hps_player: shm publicada (%u pistas)\n", prod.ntracks);
        }
        uint32_t cmds = prod.commands;
        if (!producer_poll(&prod))
            sleep_us(1000);
        if (prod.commands != cmds)
            last = now_ms();              /* PLAY espera el primer bloque a proposito */
    }
    producer_stop(&prod);
    devmem_close(&d);
    return 0;
}
