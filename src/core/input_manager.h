#pragma once

#include <SDL2/SDL.h>

namespace core {

/**
 * @brief Lightweight helper to centralize input timestamps and idle detection.
 */
class InputManager {
public:
    void recordEvent(const SDL_Event& event);
    void recordKeyboardState(const Uint8* keyState);

    double lastInputSeconds() const { return lastInputSeconds_; }
    void setLastInputSeconds(double seconds) { lastInputSeconds_ = seconds; }

private:
    double lastInputSeconds_ = 0.0;
};

} // namespace core
