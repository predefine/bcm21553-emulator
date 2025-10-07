#include <devices.h>
#include <unicorn/unicorn.h>

DEVICE(INTC, {
    .address = 0x08810000,
    .size = 0x1000,
});
