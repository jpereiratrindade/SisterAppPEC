#include "init_sdl.h"

#include <cstdio>

bool initSDL(SDLContext& sdl, const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "Erro SDL: %s\n", SDL_GetError());
        return false;
    }

    sdl.window = SDL_CreateWindow(title,
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  width, height,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!sdl.window) {
        std::fprintf(stderr, "Erro ao criar janela SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    return true;
}

void destroySDL(SDLContext& sdl) {
    if (sdl.window) {
        SDL_DestroyWindow(sdl.window);
        sdl.window = nullptr;
    }
    SDL_Quit();
}
