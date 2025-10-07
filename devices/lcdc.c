#include "log.h"
#include <devices.h>
#include <signal.h>
#include <string.h>
#include <unicorn/unicorn.h>
#include <pthread.h>
#include <raylib.h>

#define WIDTH 240
#define HEIGHT 320
#define FPS 60

// 128 kb, 240*320*4 = 75 kb, add extra space for some cases
#define DATA_BUFFER_SIZE 128 * 1024

typedef struct {
    uint32_t status;
    uint32_t cmdr;
    uint32_t command_waits_data;
    uint32_t data_buffer_count;
    uint32_t data_buffer[DATA_BUFFER_SIZE];

    // dma
    struct {
        uint16_t start;
        uint16_t end;
    } width_desc, height_desc;

    // render
    uint8_t fb_init_done;
    uint8_t update;
    unsigned char* fb_buf;
    Texture2D fb_texture;
    pthread_t render_thread;
} lcdc_instance_t;

static lcdc_instance_t lcdc_instance;

#define NEEDS_DATA(count) do { if (lcdc_instance.data_buffer_count < (uint32_t)(count)) {lcdc_instance.command_waits_data = 1; return; } } while(0)

void handle_command(uint32_t cmdr)
{
    switch (cmdr)
    {
        // skip
        case 0x04:
            break;
        case 0x2a:
        {
            NEEDS_DATA(4);
            lcdc_instance.width_desc.start = lcdc_instance.data_buffer[0] << 8 | lcdc_instance.data_buffer[1];
            lcdc_instance.width_desc.end = lcdc_instance.data_buffer[2] << 8 | lcdc_instance.data_buffer[3];
            break;
        }
        case 0x2b:
        {
            NEEDS_DATA(4);
            lcdc_instance.height_desc.start = lcdc_instance.data_buffer[0] << 8 | lcdc_instance.data_buffer[1];
            lcdc_instance.height_desc.end = lcdc_instance.data_buffer[2] << 8 | lcdc_instance.data_buffer[3];
            break;
        }
        case 0x2c:
        {
            NEEDS_DATA(
                (lcdc_instance.width_desc.end - lcdc_instance.width_desc.start + 1) *
                (lcdc_instance.height_desc.end - lcdc_instance.height_desc.start + 1)
            );

            uint32_t counter = 0;
            for(int y = lcdc_instance.height_desc.start; y <= lcdc_instance.height_desc.end; y++)
                for(int x = lcdc_instance.width_desc.start; x <= lcdc_instance.width_desc.end; x++)
                    {
                        // i hate rgb888(24 bit per pixel)
                        uint32_t fb_offset = y * WIDTH + x;
                        ((uint8_t*)lcdc_instance.fb_buf)[fb_offset * 3 + 0] = lcdc_instance.data_buffer[counter] >> 16;
                        ((uint8_t*)lcdc_instance.fb_buf)[fb_offset * 3 + 1] = lcdc_instance.data_buffer[counter] >> 8;
                        ((uint8_t*)lcdc_instance.fb_buf)[fb_offset * 3 + 2] = lcdc_instance.data_buffer[counter] >> 0;
                        counter++;
                    }

            break;
        }
        default:
            PANIC_MSG("UNKNOWN CMDR %x\n", cmdr);
            break;
    }
}

void* lcdc_render(void* arg)
{
    lcdc_instance_t* instance = arg;
    InitWindow(WIDTH, HEIGHT, "BCM21553 Emulator");
    SetTargetFPS(FPS);
    Image fb_img = {
        .data = instance->fb_buf,
        .width = WIDTH,
        .height = HEIGHT,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
        .mipmaps = 1
    };
    instance->fb_texture = LoadTextureFromImage(fb_img);
    instance->fb_init_done = 1;

    while(instance->update && !WindowShouldClose())
    {
        UpdateTexture(instance->fb_texture, instance->fb_buf);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(instance->fb_texture, 0, 0, WHITE);
        EndDrawing();
        WaitTime(((double)1) / FPS);
    }
    UnloadTexture(instance->fb_texture);
    free(instance->fb_buf);
    if (instance->update)
        raise(SIGINT);

    return NULL;
}

void lcdc_init(uc_engine* uc, void* devptr)
{
    (void)uc;
    (void)devptr;
    lcdc_instance.fb_buf = malloc(WIDTH * HEIGHT * 3);
    memset(lcdc_instance.fb_buf, 0, WIDTH * HEIGHT * 3);
    pthread_attr_t render_attr;
    lcdc_instance.update = 1;
    lcdc_instance.fb_init_done = 0;
    pthread_attr_init(&render_attr);
    pthread_create(&lcdc_instance.render_thread, &render_attr, lcdc_render, &lcdc_instance);
    pthread_detach(lcdc_instance.render_thread);
    while(!lcdc_instance.fb_init_done);
}

void lcdc_callback (uc_engine* uc, uc_mem_type type, uint64_t address, int size, long valuel, void* user_data)
{
    device* dev = (device*) user_data;
    uint64_t reg = ((address - dev->address)>>1)<<1;
    (void)size;

    if (type == UC_MEM_READ)
    {
        uc_mem_read(uc, address, &valuel, sizeof(valuel));
    }
    uint32_t value = valuel;

    switch (reg)
    {
        case 0x0: //REG_LCDC_CMDR
            lcdc_instance.status = 1 << 31; // LCDC_STATUS_FFEMPTY
            lcdc_instance.cmdr = value;
            DEBUG_MSG("lcdc: CMDR %x\n", value);
            lcdc_instance.data_buffer_count = 0;
            handle_command(value);
            break;
        case 0x4: //REG_LCDC_DATA
            lcdc_instance.status = 1 << 31; // LCDC_STATUS_FFEMPTY
            if (type == UC_MEM_WRITE)
            {
                if (lcdc_instance.command_waits_data)
                {
                    if (lcdc_instance.data_buffer_count >= DATA_BUFFER_SIZE)
                        PANIC_MSG("lcdc: data overflow(cmdr: %x\n)\n", lcdc_instance.cmdr);
                    lcdc_instance.data_buffer[lcdc_instance.data_buffer_count++] = value;
                    handle_command(lcdc_instance.cmdr);
                }
            }
            break;
        case 0x8: //REG_LCDC_RRQ
            lcdc_instance.status = 1 << 14; // LCDC_STATUS_RREADY
            break;
        case 0x10: //REG_LCDC_WTR
        case 0x14: //REG_LCDC_RTR
        case 0x18: // REG_LCDC_CR
            break;
        case 0x1c: // REG_LCDC_STATUS
        {
            // LCDC_STATUS_FFEMPTY
            uint32_t tmp = lcdc_instance.status;
            uc_mem_write(uc, address, &tmp, sizeof(tmp));
            break;
        }
        default:
            PANIC_MSG("[%s] Register 0x%lx is %s with value 0x%x, %d\n", dev->name, reg, type == UC_MEM_READ ? "read" : "wrote", value, size);
            break;
    }
}

DEVICE(LCDC, {
    .address = 0x08030000,
    .size = 0x1000,
    .callback = lcdc_callback,
    .init = lcdc_init,
});
