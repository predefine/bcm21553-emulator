#include <devices.h>
#include <unicorn/unicorn.h>

// wdt is at 0x088A0000 and 0x088A0010
// but i2c starts after wdt without 0x1000 align
DEVICE(I2C_WDT_1, {
    .address = 0x088A0000,
    .size = 0x1000,
});
