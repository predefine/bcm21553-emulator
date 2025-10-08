#include <devices.h>
#include <unicorn/unicorn.h>
#include <log.h>
#include <irq.h>

#define USB_HSOTG_INTR_RSTDONE 0x1000
#define USB_HSOTG_INTR_ENUMDONE 0x2000
#define USB_HSOTG_INTR_IN_XFERCOMPL 0x40000
#define USB_HSOTG_INTR_OUT_XFERCOMPL 0x80000

struct {
    uint32_t interrupts;
    uint32_t interrupts_mask;
} usb_instance;

void usb_reset(uc_engine* uc)
{
    usb_instance.interrupts |= USB_HSOTG_INTR_RSTDONE;

    // FIXME: that should be fired on other register write
    usb_instance.interrupts |= USB_HSOTG_INTR_ENUMDONE;

    emu_make_irq(uc);
}

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
            if (type == UC_MEM_WRITE && value & 1)
            {
                usb_reset(uc);

                // PANIC_MSG("urmom");
            }

            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        }
        case 0x14:
        {
            if (type == UC_MEM_WRITE)
                usb_instance.interrupts &= ~value;

            uc_mem_write(uc, address, &usb_instance.interrupts, sizeof(usb_instance.interrupts));
            break;
        }
        case 0x18:
        {
            if (type == UC_MEM_WRITE)
            {
                usb_instance.interrupts_mask = value;
                if (usb_instance.interrupts & usb_instance.interrupts_mask)
                    emu_make_irq(uc);
            }
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
