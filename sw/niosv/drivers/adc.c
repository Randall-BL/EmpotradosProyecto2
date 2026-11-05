#include "adc.h"
#include "hw.h"

#define ADC_REG_UPDATE      0x0
#define ADC_REG_AUTO_UPDATE 0x4

void adc_init(uintptr_t base)
{
    HW_REG(base, ADC_REG_AUTO_UPDATE) = 1;
}

uint32_t adc_read(uintptr_t base, uint32_t ch)
{
    return HW_REG(base, (ch & 7u) * 4u) & 0xFFFu;
}
