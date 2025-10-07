#include <log.h>
#include <devices.h>
#include <stdio.h>
#include <termios.h>
#include <unicorn/unicorn.h>
#include <fcntl.h>

void serial_uart_callback (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    device* dev = (device*) user_data;
    (void)dev;
    (void)uc;
    uint64_t reg = address - dev->address;
    (void)size;

    if (type == UC_MEM_WRITE)
    {
        if (reg == 0 && value != 0)
        {
            putchar((char)value);
        }
    }
    else if (type == UC_MEM_READ)
    {
        if (reg == 0x14)
        {
            uint32_t temp = 1 << 5;
            uc_mem_write(uc, address, &temp, sizeof(uint32_t));
        }
    }
}

DEVICE(SERIAL_UART_A, {
    .address = 0x08820000,
    .size = 0x1000,
    .callback = serial_uart_callback,
});

DEVICE(SERIAL_UART_B, {
    .address = 0x08821000,
    .size = 0x1000,
    .callback = serial_uart_callback,
});
