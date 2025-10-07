#include <stdint.h>
#include <unicorn/unicorn.h>
#include <log.h>
#include <devices.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <hacks.h>

typedef struct {
    int fd;
    uint32_t size;
    uint32_t block_size;
    uint32_t blocks;
} onenand_image;

typedef struct {
    uint32_t start_addr_1;
    uint32_t start_addr_2;
    uint32_t start_addr_e;
    uint32_t int_reg;

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

void onenand_callback (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long valuel, void* user_data)
{
    device* dev = (device*) user_data;
    uint64_t reg = ((address - dev->address)>>1)<<1;

    if (reg >= 0x400 && reg <= 0x400 + (1024 * 4))
        return;
    if (reg >= 0x10020 && reg <= 0x100a0)
            return;

    if (type == UC_MEM_READ)
    {
        uc_mem_read(uc, address, &valuel, sizeof(valuel));
    }
    uint32_t value = valuel;
    switch (reg)
    {
        // UNKNOWN, ignore
        case 0x0:
        case 0x4:
        case 0x14:
        case 0x16:
        case 0x1e006:
        case 0x1e442:
        case 0x1e480:
        case 0x1e498:
        case 0x1fe00:
        case 0x1fe02:
        case 0x1fe04:
        case 0x1fe06:
            // DEBUG_MSG("[%s] Register 0x%lx is %s with value 0x%x, %d\n", dev->name, reg, type == UC_MEM_READ ? "readed" : "writed", value, size);
            break;
        // UNKNOWN, hardcode
        case 0x1e004:
            value = 0x31;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0x1e00a:
            value = 1 << 8;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        // KNOWN
        case 0x1e000: // Manufactorer ID
            value = 0xec;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0x1e002:  // Device ID
            value = 0x50;
            uc_mem_write(uc, address, &value, sizeof(value));
            break;
        case 0x1e200:
            onenand_instance.start_addr_1 = value;
            break;
        case 0x1e202:
            onenand_instance.start_addr_2 = value;
            break;
        case 0x1e20e:
            onenand_instance.start_addr_e = value;
            break;
        case 0x1e400:
            if (value != 0x800)
                PANIC_MSG("WTF?");
            break;
        case 0x1e440: // cmd register
            // DEBUG_MSG("ONENAND CMD %x\n", value);
            switch (value)
            {
                case 0: // read

                    if (onenand_instance.nand.fd == 0)
                    {
                        PANIC_MSG("onenand: fd of nand image is 0\n");
                    }

                    uint32_t block = (onenand_instance.start_addr_1 << 6) + (onenand_instance.start_addr_e >> 2);
                    if (onenand_instance.start_addr_2 & (1 << 15))
                    {
                        DEBUG_MSG("second die\n");
                        block |= (1 << 15);
                    }

                    void* nand_buffer = onenand_image_block_allocbuf(&onenand_instance.nand);
                    onenand_image_block_read(&onenand_instance.nand, block, nand_buffer);

                    void* oob_buffer = onenand_image_block_allocbuf(&onenand_instance.oob);
                    onenand_image_block_read(&onenand_instance.oob, block, oob_buffer);

                    for(int i = 0; i < 0x1000; i++)
                        uc_mem_write(uc, dev->address + 0x400 + i, nand_buffer + i, sizeof(uint8_t));
                    for(int i = 0; i < 0x80; i++)
                        uc_mem_write(uc, dev->address + 0x10020 + i, oob_buffer + i, sizeof(uint8_t));

                    free(nand_buffer);
                    free(oob_buffer);

                    onenand_instance.int_reg = 0x8080;

                    {
                        uint32_t r_pc;
                        uc_reg_read(uc, UC_ARM_REG_PC, &r_pc);
                        DEBUG_MSG("READ ONENAND PC: 0x%x AT 0x%.8x, of %dKB\n", r_pc, block * 4096, block * 4096 / 1024);
                    }
                    do_hacks(uc);

                    break;
                case 0x23:
                case 0x27:
                case 0x65:
                    onenand_instance.int_reg = 0x8000;
                    break;
                case 0x7f:
                    onenand_instance.int_reg = 0x8040;
                    break;
                case 0x80:
                    {
                        uint32_t block = (onenand_instance.start_addr_1 << 6) + (onenand_instance.start_addr_e >> 2);
                        if (onenand_instance.start_addr_2 & (1 << 15))
                        {
                            DEBUG_MSG("second die2\n");
                            block |= (1 << 15);
                        }

                        void* nand_buffer = onenand_image_block_allocbuf(&onenand_instance.nand);
                        void* oob_buffer = onenand_image_block_allocbuf(&onenand_instance.oob);

                        for(int i = 0; i < 0x1000; i++)
                            uc_mem_read(uc, dev->address + 0x400 + i, nand_buffer + i, sizeof(uint8_t));

                        for(int i = 0; i < 0x80; i++)
                            uc_mem_read(uc, dev->address + 0x10020 + i, oob_buffer + i, sizeof(uint8_t));

                        onenand_image_block_write(&onenand_instance.nand, block, nand_buffer);
                        onenand_image_block_write(&onenand_instance.oob, block, oob_buffer);

                        free(nand_buffer);
                        free(oob_buffer);
                    }
                    onenand_instance.int_reg = 0x8040;
                    break;
                case 0x94:
                    onenand_image_block_erase(&onenand_instance.nand, onenand_instance.start_addr_1);
                    onenand_image_block_erase(&onenand_instance.oob, onenand_instance.start_addr_1);
                    onenand_instance.int_reg = 0x8020;
                    break;
                case 0xf0:
                    onenand_instance.int_reg = 0x8010;
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
        case 0x1e482: // interrupt register
            uc_mem_write(uc, address, &onenand_instance.int_reg, sizeof(onenand_instance.int_reg));
            break;
        case 0x1e49c:
            value = 1 << 2;
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

    onenand_image_open(&onenand_instance.nand, "emmc_build/emmc.img", 4096);
    onenand_image_open(&onenand_instance.oob, "emmc_build/emmc_oob.img", 128);
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
