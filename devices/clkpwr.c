#include <devices.h>
#include <unicorn/unicorn.h>

DEVICE(CLKPWR, {
    .address = 0x08140000,
    .size = 0x1000,
});
