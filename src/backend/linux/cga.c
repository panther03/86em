#include "cga.h"
#include <pthread.h>
#include <stdbool.h>

#include "vm_mem.h"

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
            uint8_t cc = load_u8(CGA_MEM_ADDR + cc_ind);
            uint8_t attr = load_u8(CGA_MEM_ADDR + attr_ind);

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
    while (running)
    {
        while (SDL_PollEvent(&e) > 0)
        {
            if (e.type == SDL_QUIT)
            {
                running = false;
            }
        }

        if (cga_state.mode & 0b10)
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