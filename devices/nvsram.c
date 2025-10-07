#include <devices.h>
#include <unicorn/unicorn.h>

DEVICE(NVSRAM, {
    .address = 0x08090000,
    .size = 0x1000,
});
