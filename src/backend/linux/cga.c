#include "cga.h"
#include <SDL2/SDL_events.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "vm_mem.h"
#include "kbd.h"
#include "main.h"

const uint8_t SDL_to_PS2_scancode[] = {
    /* SDL_SCANCODE_A to SDL_SCANCODE_Z */
    [4] = 0x1e,  // A
    [5] = 0x30,  // B
    [6] = 0x2e,  // C
    [7] = 0x20,  // D
    [8] = 0x12,  // E
    [9] = 0x21,  // F
    [10] = 0x22, // G
    [11] = 0x23, // H
    [12] = 0x17, // I
    [13] = 0x24, // J
    [14] = 0x25, // K
    [15] = 0x26, // L
    [16] = 0x32, // M
    [17] = 0x31, // N
    [18] = 0x18, // O
    [19] = 0x19, // P
    [20] = 0x10, // Q
    [21] = 0x13, // R
    [22] = 0x1f, // S
    [23] = 0x14, // T
    [24] = 0x16, // U
    [25] = 0x2f, // V
    [26] = 0x11, // W
    [27] = 0x2d, // X
    [28] = 0x15, // Y
    [29] = 0x2c, // Z

    /* SDL_SCANCODE_1 to SDL_SCANCODE_0 */
    [30] = 0x02, // 1
    [31] = 0x03, // 2
    [32] = 0x04, // 3
    [33] = 0x05, // 4
    [34] = 0x06, // 5
    [35] = 0x07, // 6
    [36] = 0x08, // 7
    [37] = 0x09, // 8
    [38] = 0x0a, // 9
    [39] = 0x0b,  // 0

    // space
    [44] = 0x39,
    // enter
    [40] = 0x1c,
    // backspace
    [42] = 0x0e,

    // lol
    [52] = 0xff,

    // function keys
    [58] = 0x3b,
    [59] = 0x3c,
    [60] = 0x3d,
    [61] = 0x3e,
    [62] = 0x3f,
    [63] = 0x40,
    [64] = 0x41,
    [65] = 0x42,
    [66] = 0x43,
    [67] = 0x44
};

volatile sig_atomic_t stop_flag = 0;

cga_state_t cga_state;

#define BYTE_MASK(x, b) (-((x >> b) & 1))

static inline void textmode_update(uint32_t *screen)
{
    for (int i = 0; i < 25; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            int cc_ind = (40 * i + j) << 1;
            int attr_ind = cc_ind + 1;
            uint8_t cc = load_u8_direct(CGA_COLOR_ADDR + cc_ind);
            uint8_t attr = load_u8_direct(CGA_COLOR_ADDR + attr_ind);

            uint32_t c_f = 0x7F000000 + ((attr & 0x08) ? 0x7F7F7F : 0) + ((BYTE_MASK(attr, 2) & 0x7F) << 16) + ((BYTE_MASK(attr, 1) & 0x7F) << 8) + (BYTE_MASK(attr, 0) & 0x7F);
            uint32_t c_b = 0x7F000000 + ((attr & 0x80) ? 0x7F7F7F : 0) + ((BYTE_MASK(attr, 6) & 0x7F) << 16) + ((BYTE_MASK(attr, 5) & 0x7F) << 8) + (BYTE_MASK(attr, 4) & 0x7F);
            for (int ci = 0; ci < 8; ci++)
            {
                uint8_t row = cga_state.font[cc * 8 + ci];
                for (int cj = 0; cj < 8; cj++)
                {
                    screen[(i * 8 + ci) * 40 * 8 + j * 8 + cj] = (row & 0x80) ? c_f : c_b;
                    row <<= 1;
                }
            }
        }
    }
}

static inline void cga_loop()
{
    SDL_Event e;
    bool running = true;
    uint32_t *screen = (uint32_t *)malloc(320 * 200 * sizeof(uint32_t));
    while (running && !stop_flag)
    {
       while (SDL_PollEvent(&e) > 0)
       {
           if (e.type == SDL_QUIT) {
               running = false;
           } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                uint8_t sc = e.key.keysym.scancode;
                if (sc < 4 || sc > 67) {
                    printf("unrecognized scancode %d\n", sc);
                } else {
                    uint8_t converted = SDL_to_PS2_scancode[sc];
                    if (converted == 0xFF) {
                        kbd_push_scancode(0x2a| (e.type == SDL_KEYUP ? 0x80 : 0x00));
                        kbd_push_scancode(0x28| (e.type == SDL_KEYUP ? 0x80 : 0x00));
                    } else {
                        kbd_push_scancode(converted | (e.type == SDL_KEYUP ? 0x80 : 0x00));
                    }
                    
                }
           }
       }

       if (cga_state.mode & 0x2)
       {
           // Graphics mode
           // TBD
       }
       else
       {
           // Text mode
           textmode_update(screen);
       }

       SDL_UpdateTexture(cga_state.tex, NULL, screen, 320 * sizeof(uint32_t));
       SDL_RenderClear(cga_state.renderer);
       SDL_RenderCopy(cga_state.renderer, cga_state.tex, NULL, NULL);
       SDL_RenderPresent(cga_state.renderer);
    }
    free(screen);
    SDL_DestroyRenderer(cga_state.renderer);
    SDL_DestroyWindow(cga_state.window);
    SDL_Quit();
}

static inline void cga_init()
{
    // Open the SDL Window
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Failed to initialize SDL\n");
        exit(1);
    }

    int height = 200;
    // TODO high res mode
    int width = 320;
    SDL_Window *window = SDL_CreateWindow("86em",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          320, 200,
                                          0);

    if (!window)
    {
        printf("Failed to create SDL window\n");
        exit(1);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        printf("Failed to create SDL renderer\n");
        exit(1);
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture)
    {
        printf("Failed to create SDL texture\n");
        exit(1);
    }

    FILE *font_f = fopen("roms/cgatext.bin", "r");
    if (!font_f)
    {
        printf("Failed to open CGA font ROM: cgatext.bin\n");
        exit(1);
    }

    cga_state.font = (uint8_t *)malloc(sizeof(uint8_t) * CGA_FONTROM_SIZE);
    size_t n = fread(cga_state.font, 1, CGA_FONTROM_SIZE, font_f);
    if (n != CGA_FONTROM_SIZE)
    {
        printf("Invalid font ROM, read %ld bytes instead of expected 2048\n", n);
        exit(1);
    }

    cga_state.mode = 0;
    cga_state.lock = SDL_CreateMutex();
    cga_state.renderer = renderer;
    cga_state.window = window;
    cga_state.tex = texture;
}

void *cga_thread(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());

    cga_init();

    cga_loop();

    pthread_exit(NULL);
}

void cga_start()
{
    pthread_t ptid;
    pthread_create(&ptid, NULL, cga_thread, NULL);
}