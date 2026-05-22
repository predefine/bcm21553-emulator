#include <stdint.h>
#include <unicorn/unicorn.h>
#include <log.h>
#include <devices.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <hacks.h>

// CREDITS:
// * Samsung(TM) OneNAND512(KFG1216x2A-xxB6) specification

typedef struct {
    int fd;
    uint32_t size;
    uint32_t block_size;
    uint32_t blocks;
} onenand_image;

typedef struct {
    uint32_t start_addr_1;
    uint32_t start_addr_e;
    uint32_t int_reg;

    // 0x1e400
    // BSA = [11:8]
    // BSA[3] Selection bit between BootRAM and DataRAM
    // BSA[2] Selection bit between DataRAM0 and DataRAM1
    uint8_t bsa_is_dataram;
    uint8_t bsa_dataram;

    uint32_t writesize;
    uint32_t oobsize;
    onenand_image nand;
    onenand_image oob;
} onenand_device;

static onenand_device onenand_instance;

#define calculate_blocks(value, blocksize) (value / blocksize + (value % blocksize != 0))

void onenand_image_open(onenand_image* image, char* file, uint32_t block_size)
{
    if (image->fd != 0)
        return;

    image->fd = open(file, O_RDWR);
    image->size = 0;
    image->block_size = block_size;

    if (image->fd > 0)
    {
        struct stat st;
        fstat(image->fd, &st);
        image->size = st.st_size;
    }

    image->blocks = calculate_blocks(image->size, image->block_size);
}

size_t onenand_image_read(onenand_image* image, uint32_t offset, void* buffer, size_t amount)
{
    if (offset > image->size)
    {
        memset(buffer, 0xff, amount);
        return amount;
    }

    lseek(image->fd, offset, SEEK_SET);
    return read(image->fd, buffer, amount);
}

void onenand_image_write(onenand_image* image, uint32_t offset, void* buffer, size_t amount)
{
    if ((offset + amount) > image->size)
    {
        PANIC_MSG("%s: attemp to write %ld bytes at %x(block %d) while image size is %d(%d blocks)\n", __func__,
                  amount, offset, calculate_blocks(offset, image->block_size), image->size, calculate_blocks(image->size, image->block_size));
        return;
    }
    lseek(image->fd, offset, SEEK_SET);
    write(image->fd, buffer, amount);
}

void* onenand_image_block_allocbuf(onenand_image* image)
{
    return malloc(image->block_size);
}

size_t onenand_image_block_read(onenand_image* image, uint32_t block, void* buffer)
{
    return onenand_image_read(image, block * image->block_size, buffer, image->block_size);
}

void onenand_image_block_write(onenand_image* image, uint32_t block, void* buffer)
{
    onenand_image_write(image, block * image->block_size, buffer, image->block_size);
}

void onenand_image_block_erase(onenand_image* image, uint32_t block)
{
    void* block_ff = malloc(image->block_size);
    memset(block_ff, 0xff, image->block_size);
    onenand_image_write(image, block * image->block_size, block_ff, image->block_size);
    free(block_ff);
}

#define ONENAND_INT_MASTER 0x8000
#define ONENAND_INT_READ 0x80

uint32_t onenand_addr()
{

    uint32_t block = (onenand_instance.start_addr_1 << 6) + (onenand_instance.start_addr_e >> 2);
    return block * onenand_instance.writesize;
}

uint32_t onenand_get_bufferram_main()
{
    if (!onenand_instance.bsa_is_dataram)
        return 0x0; // BootRAM

    if (onenand_instance.bsa_dataram)
        return 0xc00; // DataRAM1
    else
        return 0x400; // DataRAM0
}

uint32_t onenand_get_bufferram_spare()
{
    if (!onenand_instance.bsa_is_dataram)
        return 0x10000; // BootRAM

    if (onenand_instance.bsa_dataram)
        return 0x10060; // DataRAM1
    else
        return 0x10020; // DataRAM0
}

void onenand_callback (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long valuel, void* user_data)
{
    device* dev = (device*) user_data;
    uint64_t reg = ((address - dev->address)>>1)<<1;

    // BootRam area
    // TODO: read first kb into that area instead of loading BcmBoot.img?
    if (reg < 0x200)
        return;

    // Main area
    if (reg < 0x10000)
        return;

    // Spare area
    if (reg < 0x12000)
        return;

    if (type == UC_MEM_READ)
        uc_mem_read(uc, address, &valuel, sizeof(valuel));
    uint32_t value = valuel;

    // if (reg == 0x1e200 || reg == 0x1e202 || reg == 0x1e20e)
    // {
    //     uint32_t r_pc;
    //     uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
    //     DEBUG_MSG("[%s] Register 0x%lx is %s with value 0x%x at PC: 0x%x, %d\n", dev->name, reg, type == UC_MEM_READ ? "read" : "writen", value, r_pc, size);
    // }
    // do_hacks(uc);
    switch (reg >> 1)
    {
        // UNKNOWN, ignore
        case 0xf221:
        case 0xf240:
        case 0xf24c:
        case 0xff00:
        case 0xff01:
        case 0xff02:
        case 0xff03:
            break;
        // UNKNOWN, hardcode
        case 0xf005:
            value = 1 << 8;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        // KNOWN
        case 0xf000: // Manufactorer ID
            value = 0xec;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0xf001:  // Device ID
            value = 0x50;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0xf002: // Version ID
            value = 0x0241;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0xf003: // Write size
            value = onenand_instance.writesize;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0xf100:
            onenand_instance.start_addr_1 = value;
            break;
        case 0xf101:
            if (value != 0)
                PANIC_MSG("0xf101(start_addr_2): 0x%x\n", value);
            break;
        case 0xf107:
            onenand_instance.start_addr_e = value;
            break;
        case 0xf200:
            onenand_instance.bsa_is_dataram = (value >> 11) & 1;
            onenand_instance.bsa_dataram = (value >> 10) & 1;
            break;
        case 0xf220: // cmd register
            // DEBUG_MSG("ONENAND CMD %x\n", value);
            switch (value)
            {
                case 0: // read

                    if (onenand_instance.nand.fd == 0)
                    {
                        PANIC_MSG("onenand: fd of nand image is 0\n");
                    }

                    uint32_t addr = onenand_addr();

                    void* nand_buffer = onenand_image_block_allocbuf(&onenand_instance.nand);
                    onenand_image_block_read(&onenand_instance.nand, addr / onenand_instance.writesize, nand_buffer);

                    void* oob_buffer = onenand_image_block_allocbuf(&onenand_instance.oob);
                    onenand_image_block_read(&onenand_instance.oob, addr / onenand_instance.writesize, oob_buffer);

                    // DATARAM0
                    for(int i = 0; i < onenand_instance.writesize; i++)
                        uc_mem_write(uc, dev->address + onenand_get_bufferram_main() + i, nand_buffer + i, sizeof(uint8_t));
                    for(int i = 0; i < onenand_instance.oobsize; i++)
                        uc_mem_write(uc, dev->address + onenand_get_bufferram_spare() + i, oob_buffer + i, sizeof(uint8_t));

                    free(nand_buffer);
                    free(oob_buffer);

                    onenand_instance.int_reg = ONENAND_INT_MASTER | ONENAND_INT_READ;

                    {
                        uint32_t r_pc;
                        uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
                        DEBUG_MSG("READ ONENAND PC: 0x%x %d %d, AT 0x%.8x, of %dKB\n", r_pc, onenand_instance.start_addr_1, onenand_instance.start_addr_e, addr, addr / 1024);
                    }
                    do_hacks(uc);

                    break;
                case 0x23: // Unlock Nand Block
                case 0x27: // Unlock Nand
                case 0x65: // OTP Access
                    onenand_instance.int_reg = ONENAND_INT_MASTER;
                    break;
                case 0x7f: // ONENAND_CMD_2X_CACHE_PROG
                    onenand_instance.int_reg = ONENAND_INT_MASTER | 0x40;
                    break;
                case 0x80: // Program single/multiple sector data unit from buffer
                    {
                        uint32_t addr = onenand_addr();

                        void* nand_buffer = onenand_image_block_allocbuf(&onenand_instance.nand);
                        void* oob_buffer = onenand_image_block_allocbuf(&onenand_instance.oob);

                        for(int i = 0; i < onenand_instance.writesize; i++)
                            uc_mem_read(uc, dev->address + onenand_get_bufferram_main() + i, nand_buffer + i, sizeof(uint8_t));

                        for(int i = 0; i < onenand_instance.oobsize; i++)
                            uc_mem_read(uc, dev->address + onenand_get_bufferram_spare() + i, oob_buffer + i, sizeof(uint8_t));

                        onenand_image_block_write(&onenand_instance.nand, addr / onenand_instance.writesize, nand_buffer);
                        onenand_image_block_write(&onenand_instance.oob, addr / onenand_instance.writesize, oob_buffer);

                        free(nand_buffer);
                        free(oob_buffer);
                    }
                    onenand_instance.int_reg = ONENAND_INT_MASTER | 0x40;
                    break;
                case 0x94: // Block erase
                    onenand_image_block_erase(&onenand_instance.nand, onenand_instance.start_addr_1);
                    onenand_image_block_erase(&onenand_instance.oob, onenand_instance.start_addr_1);
                    onenand_instance.int_reg = ONENAND_INT_MASTER | 0x20;
                    break;
                case 0xf0: // Reset NAND Flash Core
                    // TODO: reset internal state?
                    onenand_instance.int_reg = ONENAND_INT_MASTER | 0x10;
                    break;
                default:
                    {
                        uint32_t r_pc;
                        uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
                        DEBUG_MSG("PC: 0x%x\n", r_pc);
                    }
                    PANIC_MSG("UNKNOWN ONENAND CMD %x\n", value);
            }
            break;
        case 0xf241: // interrupt register
            if (type == UC_MEM_READ)
                uc_mem_write(uc, address, &onenand_instance.int_reg, sizeof(onenand_instance.int_reg));
            else
                onenand_instance.int_reg = value;
            break;
        case 0xf24e: // NAND Flash Write Protection Status Register
            value = 1 << 2; // unlocked
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        default:
            PANIC_MSG("[%s] Register 0x%lx is %s with value 0x%x, %d\n", dev->name, reg, type == UC_MEM_READ ? "readed" : "writed", value, size);
            break;
    }
}

void onenand_init(uc_engine* uc, void* devptr)
{
    (void) uc;
    device* dev = devptr;
    (void) dev;

    onenand_instance.writesize = 0x1000;
    onenand_instance.oobsize = onenand_instance.writesize >> 5;
    onenand_image_open(&onenand_instance.nand, "emmc_build/emmc.img", onenand_instance.writesize);
    onenand_image_open(&onenand_instance.oob, "emmc_build/emmc_oob.img", onenand_instance.oobsize);
}

DEVICE(ONENAND1, {
    .address = 0x00000,
    .size = 0x20000,
    .callback = onenand_callback,
    .init = onenand_init
});

DEVICE(ONENAND2, {
    .address = 0x20000,
    .size = 0x20000,
    .callback = onenand_callback,
    .init = onenand_init
});

// used in SBL
DEVICE(ONENAND3, {
    .address = 0x400000,
    .size = 0x20000,
    .callback = onenand_callback,
    .init = onenand_init
});
