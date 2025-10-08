#ifndef _IRQ_H__
#define _IRQ_H__

#include <unicorn/unicorn.h>

void emu_register_irq_hooks(uc_engine* uc);
void emu_make_irq(uc_engine* uc);

#endif
