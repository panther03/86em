#ifndef CGA_H
#define CGA_H

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL.h>
#include <pthread.h>

typedef struct {
    uint8_t mode;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *tex;
    // TODO this whole lock thing
    SDL_mutex* lock;
    uint8_t *font;
} cga_state_t;

void cga_start();

extern cga_state_t cga_state;

#define CGA_REG_MODE  0x3D8
#define CGA_REG_COLOR 0x3D9
#define CGA_REG_STATUS 0x3DA

#define CGA_MEM_ADDR 0xA4000
#define CGA_FONTROM_SIZE 2048

#define CGA_REG_START CGA_REG_MODE
#define CGA_REG_END   CGA_REG_STATUS

#endif // CGA_H