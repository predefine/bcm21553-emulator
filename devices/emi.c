#include <devices.h>
#include <unicorn/unicorn.h>

void emi_init(uc_engine* uc, void* devptr)
{
    device* dev = (device*)devptr;
    uint32_t reg_3c;
    reg_3c = 0x3f;
    uc_mem_write(uc, dev->address + 0x3c, &reg_3c, sizeof(reg_3c));
}

DEVICE(EMI, {
    .address = 0x08420000,
    .size = 0x1000,
    .init = emi_init,
});
