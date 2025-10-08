#include <unicorn/unicorn.h>
#include <irq.h>
#include <log.h>

// based on pseudocode from `ARMv6-M Architecture Reference Manual`

void irq_fffffff4_handler(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);

void emu_register_irq_hooks(uc_engine* uc)
{
    uc_err err = uc_mem_map(uc, 0xfffff000, 0x1000, UC_PROT_EXEC);
    if (err)
        PANIC_MSG("Failed to map irq memory at with error %u(%s)\n", err, uc_strerror(err));

    uc_hook irq_fffffff4_handler_hook;
    err = uc_hook_add(uc, &irq_fffffff4_handler_hook,
                      UC_HOOK_CODE, irq_fffffff4_handler, NULL, 0xfffffff4, 0xfffffff4);
    if (err)
        PANIC_MSG("failed to register irq 0xfffffff4 hook with error %u(%s)!\n", err, uc_strerror(err));
}

void emu_make_irq(uc_engine* uc)
{
    uint32_t r_sp;
    uc_reg_read(uc, UC_ARM_REG_MSP, &r_sp);
    uint32_t frameptr = (r_sp & ~ 0b111) - 0x20;

    uint32_t r_r0, r_r1, r_r2, r_r3, r_r12, r_lr, r_pc, r_xpsr, r_ctrl;
    uc_reg_read(uc, UC_ARM_REG_R0, &r_r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r_r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r_r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r_r3);
    uc_reg_read(uc, UC_ARM_REG_R12, &r_r12);
    uc_reg_read(uc, UC_ARM_REG_LR, &r_lr);
    uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
    r_pc += 4;
    uc_reg_read(uc, UC_ARM_REG_XPSR, &r_xpsr);
    uc_reg_read(uc, UC_ARM_REG_CONTROL, &r_ctrl);

    r_xpsr |= ((r_sp >> 1) & 1) << 9;

    uc_mem_write(uc, frameptr + (0 << 2), &r_r0, sizeof(r_r0));
    uc_mem_write(uc, frameptr + (1 << 2), &r_r1, sizeof(r_r1));
    uc_mem_write(uc, frameptr + (2 << 2), &r_r2, sizeof(r_r2));
    uc_mem_write(uc, frameptr + (3 << 2), &r_r3, sizeof(r_r3));
    uc_mem_write(uc, frameptr + (4 << 2), &r_r12, sizeof(r_r12));
    uc_mem_write(uc, frameptr + (5 << 2), &r_lr, sizeof(r_lr));
    uc_mem_write(uc, frameptr + (6 << 2), &r_pc, sizeof(r_pc));
    uc_mem_write(uc, frameptr + (7 << 2), &r_xpsr, sizeof(r_xpsr));

    uint32_t return_addr = (r_ctrl >> 1) & 1 ? 0xfffffffd : 0xfffffff9;
    uc_reg_write(uc, UC_ARM_REG_LR, &return_addr);
    uc_reg_write(uc, UC_ARM_REG_MSP, &frameptr);

    uint32_t r_ipsr;
    uc_reg_read(uc, UC_ARM_REG_IPSR, &r_ipsr);
    r_ipsr = (r_ipsr & ~(0b111111)) | 6;
    uc_reg_write(uc, UC_ARM_REG_IPSR, &r_ipsr);

    uc_reg_read(uc, UC_ARM_REG_CONTROL, &r_ctrl);
    r_ctrl &= ~1;
    uc_reg_write(uc, UC_ARM_REG_CONTROL, &r_ctrl);

    // FIXME: i dont know where vector table located so i use hardcoded address(0x96800000)
    // DEBUG_MSG("SP BEFORE JUMP TO IRQ HANDLER: %x\n", frameptr);
    uint32_t exception_pc = 0x96800000 + 0x18;
    uc_reg_write(uc, UC_ARM_REG_PC, &exception_pc);
}

void irq_fffffff4_handler(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    (void)uc;
    (void)address;
    (void)size;
    (void)user_data;

    uint32_t r_sp;
    uc_reg_read(uc, UC_ARM_REG_MSP, &r_sp);
    uint32_t frameptr = r_sp;

    uint32_t r_ctrl;
    uc_reg_read(uc, UC_ARM_REG_CONTROL, &r_ctrl);
    r_ctrl &= ~1;
    uc_reg_write(uc, UC_ARM_REG_CONTROL, &r_ctrl);

    uint32_t r_r0, r_r1, r_r2, r_r3, r_r12, r_lr, r_pc, r_xpsr;
    uc_mem_read(uc, frameptr + (0 << 2), &r_r0, sizeof(r_r0));
    uc_mem_read(uc, frameptr + (1 << 2), &r_r1, sizeof(r_r1));
    uc_mem_read(uc, frameptr + (2 << 2), &r_r2, sizeof(r_r2));
    uc_mem_read(uc, frameptr + (3 << 2), &r_r3, sizeof(r_r3));
    uc_mem_read(uc, frameptr + (4 << 2), &r_r12, sizeof(r_r12));
    uc_mem_read(uc, frameptr + (5 << 2), &r_lr, sizeof(r_lr));
    uc_mem_read(uc, frameptr + (6 << 2), &r_pc, sizeof(r_pc));
    uc_mem_read(uc, frameptr + (7 << 2), &r_xpsr, sizeof(r_xpsr));

    uc_reg_write(uc, UC_ARM_REG_R0, &r_r0);
    uc_reg_write(uc, UC_ARM_REG_R1, &r_r1);
    uc_reg_write(uc, UC_ARM_REG_R2, &r_r2);
    uc_reg_write(uc, UC_ARM_REG_R3, &r_r3);
    uc_reg_write(uc, UC_ARM_REG_R12, &r_r12);
    uc_reg_write(uc, UC_ARM_REG_LR, &r_lr);
    uc_reg_write(uc, UC_ARM_REG_PC, &r_pc);

    r_sp = frameptr + 0x20 + (((r_xpsr >> 9) & 1) << 1);

    uint32_t r_apsr;
    uc_reg_read(uc, UC_ARM_REG_APSR, &r_apsr);
    r_apsr &= ~(0b1111 << 28);
    r_apsr |= r_xpsr & (0b1111 << 28);
    uc_reg_write(uc, UC_ARM_REG_APSR, &r_apsr);

    uint32_t r_ipsr;
    uc_reg_read(uc, UC_ARM_REG_IPSR, &r_ipsr);
    r_ipsr &= ~(0b111111);
    r_ipsr |= r_xpsr & 0b111111;
    uc_reg_write(uc, UC_ARM_REG_IPSR, &r_ipsr);

    uint32_t r_epsr;
    uc_reg_read(uc, UC_ARM_REG_EPSR, &r_epsr);
    r_epsr &= ~(1 << 24);
    r_epsr |= r_xpsr & (1 << 28);
    uc_reg_write(uc, UC_ARM_REG_EPSR, &r_epsr);

    uc_reg_write(uc, UC_ARM_REG_MSP, &r_sp);
}
