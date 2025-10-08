#include <hacks.h>

// TODO: remove all hacks by fixing implementation of some devices
void do_hacks(uc_engine* uc)
{
    // HACK: change bcmboot's second stage loglevel
    uint32_t tmp = 0xff;
    uc_mem_write(uc, 0x948131A2, &tmp, sizeof(uint32_t));

    // HACK: bypass "MSMp" atag(needs for "CP", causes "data abort")
    tmp = 0;
    uc_mem_write(uc, 0x96801D94, &tmp, sizeof(uint32_t));

    // HACK: contains funny `strlen` call
    tmp = 0x0000a0e1;
    uc_mem_write(uc, 0x96801A8C, &tmp, sizeof(tmp));

    // MENTAL ILLNESS OR BAD `emu_make_irq` IMPLENTATION
    // R0(always 0x28000000) > SP(equals to 0x967fff68 when calling irq handler)
    uc_mem_write(uc, 0x96800030, &tmp, sizeof(tmp));
    uc_mem_write(uc, 0x96800040, &tmp, sizeof(tmp));

    // HACK: force boot to `odin` mode
    tmp = 0xe12fff1e;
    uc_mem_write(uc, 0x96811520, &tmp, sizeof(tmp));

    // HACK: force nps_status to be "STAR"(`start`?)
    // tmp = 'F' | 'A' << 8 | 'I' << 16 | 'L' << 24;
    tmp = 'S' | 'T' << 8 | 'A' << 16 | 'R' << 24;
    uc_mem_write(uc, 0x968108C0, &tmp, sizeof(tmp));

    // HACK: enable log in sbl
    tmp = 0;
    uc_mem_write(uc, 0x96A09A8C, &tmp, sizeof(tmp));
}
