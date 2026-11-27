/*
 * Acceso a registros del HPS y al puente liviano HPS->FPGA desde Linux, con mmap de
 * /dev/mem y punteros volatile (sin hwlib ni drivers; regla de CLAUDE.md / P4).
 *
 * Direcciones del Cyclone V Hard Processor System Technical Reference Manual:
 *   FPGA Manager  0xFF70_6000  stat[2:0] = modo de la FPGA (4 = usuario, configurada)
 *   Reset Manager 0xFFD0_5000  brgmodrst (+0x1C): bit 1 = reset del puente liviano
 *   Puente liviano 0xFF20_0000 shm.s2 al inicio (hw/soc/build_system.tcl)
 *
 * El registro remap del L3 (0xFF80_0000, que hace visible el puente) NO se toca desde
 * Linux: escribirlo con Linux corriendo congela el sistema (probado en la tarjeta el 12
 * nov con devmem). Lo escribe U-Boot en boot.cmd ("bridge enable" o mw.l), antes del
 * kernel.
 */
#ifndef HPS_DEVMEM_H
#define HPS_DEVMEM_H

#include <stdint.h>

/* FPGA Manager: estado de configuracion de la FPGA. */
#define FPGAMGR_BASE       0xFF706000u
#define FPGAMGR_STAT       0x000u          /* registro stat */
#define FPGAMGR_MODE_MASK  0x7u            /* stat[2:0] = modo */
#define FPGAMGR_MODE_USER  0x4u            /* modo usuario: FPGA configurada y corriendo */

/* Reset Manager: reset de los puentes HPS <-> FPGA. */
#define RSTMGR_BASE        0xFFD05000u
#define RSTMGR_BRGMODRST   0x01Cu          /* registro brgmodrst */
#define BRGMODRST_LWH2F    (1u << 1)       /* 1 = puente liviano en reset */

/* Puente liviano HPS -> FPGA y memoria compartida dentro de el. */
#define LWH2F_BASE         0xFF200000u
#define LWH2F_SHM_OFFSET   0x00000000u     /* shm esta al inicio del puente */
#define SHM_SPAN           0x10000u        /* 64 KB */

/* Mapeos abiertos sobre /dev/mem. */
typedef struct {
    int fd;                                /* descriptor de /dev/mem (-1 = cerrado) */
    volatile uint32_t *fpgamgr;            /* pagina del FPGA Manager */
    volatile uint32_t *rstmgr;             /* pagina del Reset Manager */
    volatile void *shm;                    /* los 64 KB de shm */
} devmem_t;

/* Abre /dev/mem y mapea los tres bloques. 0 si todo salio bien. */
int devmem_open(devmem_t *d);
/* Deshace los mapeos y cierra /dev/mem (se puede llamar con mapeos parciales). */
void devmem_close(devmem_t *d);

/* 1 si la FPGA esta en modo usuario (configurada): solo entonces se toca el puente. */
int devmem_fpga_ready(const devmem_t *d);

/* Saca de reset el puente liviano (U-Boot ya lo hizo visible con remap). */
void devmem_lwbridge_enable(devmem_t *d);

#endif /* HPS_DEVMEM_H */
