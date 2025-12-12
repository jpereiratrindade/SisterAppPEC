#include "input_manager.h"

namespace core {

void InputManager::recordEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        case SDL_KEYDOWN:
            lastInputSeconds_ = static_cast<double>(SDL_GetTicks()) / 1000.0;
            break;
        default:
            break;
    }
}

void InputManager::recordKeyboardState(const Uint8* keyState) {
    if (!keyState) return;
    if (keyState[SDL_SCANCODE_W] || keyState[SDL_SCANCODE_A] || keyState[SDL_SCANCODE_S] || keyState[SDL_SCANCODE_D] ||
        keyState[SDL_SCANCODE_SPACE] || keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT] ||
        keyState[SDL_SCANCODE_Q] || keyState[SDL_SCANCODE_E] || keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_DOWN] ||
        keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_RIGHT]) {
        lastInputSeconds_ = static_cast<double>(SDL_GetTicks()) / 1000.0;
    }
}

} // namespace core
