#include <devices.h>
#include <unicorn/unicorn.h>

// even better than i2c
// auxadc is at 0x20 offset
// gptimer is at 0x100 offset
// bb2pmu_adcsync is at 0x200 offset
// in same 0x0883XXX space
DEVICE(ADC_GPTIMER_BB2PMU, {
    .address = 0x08830000,
    .size = 0x1000,
});
