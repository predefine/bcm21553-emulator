#include <devices.h>
#include <unicorn/unicorn.h>

DEVICE(GPIO, {
    .address = 0x088ce000,
    .size = 0x1000,
});
