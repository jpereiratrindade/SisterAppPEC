#pragma once

#include <SDL2/SDL.h>

struct SDLContext {
    SDL_Window* window = nullptr;
};

bool initSDL(SDLContext& sdl, const char* title, int width, int height);
void destroySDL(SDLContext& sdl);
