#include <devices.h>
#include <unicorn/unicorn.h>

DEVICE(SYSCFG, {
    .address = 0x08880000,
    .size = 0x1000,
});
