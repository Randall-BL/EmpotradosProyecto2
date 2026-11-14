#include "devmem.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE 0x1000u

#define REG(base, off) ((base)[(off) / 4u])

static volatile void *map(int fd, uint32_t phys, uint32_t span)
{
    void *p = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)phys);
    return p == MAP_FAILED ? NULL : (volatile void *)p;
}

int devmem_open(devmem_t *d)
{
    d->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (d->fd < 0)
        return -1;
    d->fpgamgr = map(d->fd, FPGAMGR_BASE, PAGE);
    d->rstmgr = map(d->fd, RSTMGR_BASE, PAGE);
    d->shm = map(d->fd, LWH2F_BASE + LWH2F_SHM_OFFSET, SHM_SPAN);
    if (!d->fpgamgr || !d->rstmgr || !d->shm) {
        devmem_close(d);
        return -1;
    }
    return 0;
}

void devmem_close(devmem_t *d)
{
    if (d->fpgamgr)
        munmap((void *)d->fpgamgr, PAGE);
    if (d->rstmgr)
        munmap((void *)d->rstmgr, PAGE);
    if (d->shm)
        munmap((void *)d->shm, SHM_SPAN);
    if (d->fd >= 0)
        close(d->fd);
    d->fpgamgr = d->rstmgr = NULL;
    d->shm = NULL;
    d->fd = -1;
}

int devmem_fpga_ready(const devmem_t *d)
{
    return (REG(d->fpgamgr, FPGAMGR_STAT) & FPGAMGR_MODE_MASK) == FPGAMGR_MODE_USER;
}

void devmem_lwbridge_enable(devmem_t *d)
{
    REG(d->rstmgr, RSTMGR_BRGMODRST) &= ~BRGMODRST_LWH2F;
}
