#include <devices.h>
#include <unicorn/unicorn.h>

// TODO: needs to implement `interrrupt "calling"` for usb
void usb_hsotg_callback (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    device* dev = (device*) user_data;
    (void)dev;
    (void)uc;
    uint64_t reg = ((address - dev->address) >> 2) << 2;
    (void)size;
    switch (reg)
    {
        case 0x10:
        {
            uint32_t tmp = 0;
            uc_mem_write(uc, address, &tmp, sizeof(tmp));
            break;
        }
        default:
            break;
    }
}

DEVICE(USB_HSOTG, {
    .address = 0x08200000,
    .size = 0x1000,
    .callback = usb_hsotg_callback,
});

DEVICE(USB_HSOTG_CTRL, {
    .address = 0x08280000,
    .size = 0x1000,
});
