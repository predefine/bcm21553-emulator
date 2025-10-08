#include <unicorn/unicorn.h>
#include <irq.h>
#include <log.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <devices.h>
#include <byteswap.h>

#define BOOTVECTOR_RESET 0x28000000
// #define BOOTVECTOR_RESET SBL_OFFSET

#define SBL_FILE "../young/Sbl.bin"
#define SBL_OFFSET 0x96800000
#define SBL_SIZE 0x200000

#define BCMBOOT_FILE "../young/BcmBoot.img"
#define BCMBOOT_OFFSET (0x28000000)
#define BCMBOOT_SIZE 0x20000

#define BCMBOOT_MEM_OFFSET 0x28000000
#define BCMBOOT_MEM_SIZE 0x20000

#define RAM_OFFSET 0x80000000
#define RAM_SIZE 0x20000000

#define be64_to_le(val) __bswap_64(val)

void mem_read_unmapped(uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    (void)uc;
    (void)size;
    (void)value;
    (void)user_data;

    printf("ERROR! detected %s on unmapped 0x%lx-0x%lx\n", type == UC_MEM_READ_UNMAPPED ? "read" : "write", address, address + size);
}

#ifdef DEBUG_MEM
void mem_info(uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    (void)uc;
    (void)size;
    (void)value;
    (void)user_data;
    (void)type;

    if (type == UC_MEM_READ)
        uc_mem_read(uc, address, &value, size);

    uint32_t r_pc, r_lr;
    uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &r_lr);

    printf("INFO! detected %s at 0x%lx-0x%lx with value=0x%lx at pc=0x%x; lr=0x%x\n", type == UC_MEM_READ ? "read" : "write", address, address+size, value, r_pc, r_lr);
}
#endif

uc_engine* engine;


void emu_exit(int _)
{
    (void)_;

    printf("Emulator terminating...\n");

    uint32_t r_PC, r_LR, r_SP;
    uint32_t r_R0, r_R1, r_R2;
    uint32_t r_R3, r_R4, r_R5;
    uint32_t r_R6, r_R7, r_R8;
    uc_reg_read(engine, UC_ARM_REG_PC, &r_PC);
    uc_reg_read(engine, UC_ARM_REG_LR, &r_LR);
    uc_reg_read(engine, UC_ARM_REG_SP, &r_SP);
    uc_reg_read(engine, UC_ARM_REG_R0, &r_R0);
    uc_reg_read(engine, UC_ARM_REG_R1, &r_R1);
    uc_reg_read(engine, UC_ARM_REG_R2, &r_R2);
    uc_reg_read(engine, UC_ARM_REG_R3, &r_R3);
    uc_reg_read(engine, UC_ARM_REG_R4, &r_R4);
    uc_reg_read(engine, UC_ARM_REG_R5, &r_R5);
    uc_reg_read(engine, UC_ARM_REG_R6, &r_R6);
    uc_reg_read(engine, UC_ARM_REG_R7, &r_R7);
    uc_reg_read(engine, UC_ARM_REG_R8, &r_R8);
    printf("PC: 0x%x, LR: 0x%x, SP: 0x%x\n", r_PC, r_LR, r_SP);
    printf("R0: 0x%x; R1: 0x%x; R2: 0x%x\n", r_R0, r_R1, r_R2);
    printf("R3: 0x%x; R4: 0x%x; R5: 0x%x\n", r_R3, r_R4, r_R5);
    printf("R6: 0x%x; R7: 0x%x; R8: 0x%x\n", r_R6, r_R7, r_R8);

    devices_list* devices = (devices_list*)get_devices();

    for(;;)
    {
        device* dev = devices->this;

        if (dev->exit)
            dev->exit(engine, (void*)dev);

        devices = devices->next;
        if (!devices)
            break;
    }

    exit(0);
}

void emu_start(uint32_t start)
{
    printf("Starting emulator...\n");

    signal(SIGINT, emu_exit);

    uc_err err = uc_emu_start(engine, start, 0x0, 0, 0);
    if (err)
    {
        printf("Failed on uc_emu_start() with error returned %u: %s\n", err, uc_strerror(err));
    }

    uint32_t r_pc;
    uc_reg_read(engine, UC_ARM_REG_PC, &r_pc);
    printf("PC: 0x%x\n", r_pc);
}

void emu_init()
{
    if (uc_open(UC_ARCH_ARM, UC_MODE_LITTLE_ENDIAN, &engine) != UC_ERR_OK)
    {
        printf("uc_open failed :(\n");
        exit(-1);
    }

    if (uc_ctl_set_cpu_model(engine, UC_CPU_ARM_1136) != UC_ERR_OK)
    {
        printf("uc_ctl_set_cpu_model failed :(\n");
        exit(-1);
    }
}

void map_mem(uint64_t address, uint64_t size, char ignore_err)
{
    uc_err err = uc_mem_map(engine, address, size, UC_PROT_ALL);
    if (err && !ignore_err)
        PANIC_MSG("Failed map memory at 0x%lx, size 0x%lx with error returned %u: %s\n", address, size, err, uc_strerror(err));
}

void load_file(char* filename, uint64_t address, uint64_t size)
{
    int fd;
    char *data = (char*)malloc(size);

    if ((fd = open(filename, O_RDONLY)) < 0)
        PANIC_MSG("load_file: open file `%s` failed :(\n", filename);

    if (read(fd, data, size) < 1)
        PANIC_MSG("load_file: read file `%s` failed :(\n", filename);

    close(fd);

    uc_err err = uc_mem_write(engine, address, data, size);
    if (err)
        PANIC_MSG("load_file: Failed write memory with error returned %u: %s\n", err, uc_strerror(err));

    DEBUG_MSG("load_file: memory writed!\n");
    free(data);
}

void map_mem_and_load_file(char* filename, uint64_t address, uint64_t size)
{
    map_mem(address, size, 1);
    load_file(filename, address, size);
}

void devices_probe()
{
    devices_list* devices = (devices_list*)get_devices();

    for(;;)
    {
        device* dev = devices->this;

        DEBUG_MSG("Adding device %s...\n", dev->name);

        map_mem(dev->address, dev->size, 1);
        if (dev->callback)
        {
            DEBUG_MSG("Adding device callback...\n");

            uc_hook* hook = malloc(sizeof(uc_hook));
            uc_err err = uc_hook_add(engine, hook, UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, dev->callback, dev, dev->address,
                                     dev->address + dev->size - 1);
            if (err)
                DEBUG_MSG("Adding device hook failed with error %u!\n", err);
        }

        if (dev->init)
            dev->init(engine, (void*)dev);

        devices = devices->next;
        if (!devices)
            break;
    }
}

void mem_console(uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    (void)uc;
    (void)size;
    (void)value;
    (void)user_data;
    (void)type;

    uint32_t r_pc;
    uc_reg_read(engine, UC_ARM_REG_PC, &r_pc);
    if (address >= 0x968F5890 && address <= 0x968F5890 + 0xFFFFF)
        if (r_pc >= 0x9680376C && r_pc <= 0x96803800) // HACK: handle memory_console writes only at specific code position
            putchar(value);
}

void exception_handler(uc_engine* uc, uint32_t intr, void* user_data)
{
    (void)uc;
    (void)user_data;

    uint32_t r_pc, r_r0, r_r1, r_lr;
    uc_reg_read(engine, UC_ARM_REG_PC, &r_pc);
    uc_reg_read(engine, UC_ARM_REG_R0, &r_r0);
    uc_reg_read(engine, UC_ARM_REG_R1, &r_r1);
    uc_reg_read(engine, UC_ARM_REG_LR, &r_lr);
    printf("EXCEPTION 0x%x(at 0x%x) AT PC: 0x%x\n", intr, intr << 2, r_pc);
    printf("R0: %x, R1: %x, LR: %x\n", r_r0, r_r1, r_lr);

    emu_exit(-2);
}

void add_hooks()
{
    uc_hook mem_unmapped_read_hook;
    uc_err err = uc_hook_add(engine, &mem_unmapped_read_hook,
                             UC_HOOK_MEM_UNMAPPED, mem_read_unmapped, NULL, 1, 0);
    if (err)
        printf("unmapped read&write hook add failed with error %u %s!\n", err, uc_strerror(err));

    #ifdef DEBUG_MEM
    uc_hook mem_info_hook;
    err = uc_hook_add(engine, &mem_info_hook,
                      UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, mem_info, NULL, 1, 0);
    if (err)
        printf("mem read&write hook add failed with error %u %s!\n", err, uc_strerror(err));
    #endif

    uc_hook mem_console_hook;
    err = uc_hook_add(engine, &mem_console_hook,
                      UC_HOOK_MEM_WRITE, mem_console, NULL, 1, 0);
    if (err)
        printf("mem console hook add failed with error %u %s!\n", err, uc_strerror(err));

    uc_hook exception_handler_hook;
    err = uc_hook_add(engine, &exception_handler_hook,
                      UC_HOOK_INTR, exception_handler, NULL, 1, 0);
    if (err)
        printf("exception handler hook add failed with error %u %s!\n", err, uc_strerror(err));

    emu_register_irq_hooks(engine);
}

int main()
{
    emu_init();

    add_hooks();
    // RAM
    map_mem(RAM_OFFSET, RAM_SIZE, 0);

    load_file(SBL_FILE, SBL_OFFSET, SBL_SIZE);

    devices_probe(); // bcmboot lives in scratchram
    load_file(BCMBOOT_FILE, BCMBOOT_OFFSET, BCMBOOT_SIZE);


    emu_start(BOOTVECTOR_RESET);
    emu_exit(-1);

    return 0;
}


// HACK: force boot without "onkey"
void scratchram_handler (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long value, void* user_data)
{
    device* dev = (device*) user_data;
    (void)type;
    (void)dev;
    (void)uc;
    (void)value;
    (void)size;

    if (address == 0x28004000)
    {
        uint32_t temp = 0x66262564;
        uc_mem_write(uc, address, &temp, sizeof(uint32_t));
    }
}

DEVICE(SCRATCH_RAM, {.address = BCMBOOT_OFFSET, .size = BCMBOOT_SIZE, .callback = scratchram_handler});
