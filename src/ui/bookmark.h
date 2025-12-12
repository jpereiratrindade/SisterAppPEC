#pragma once

#include "../math/math_types.h"
#include "../graphics/camera.h"

#include <string>

namespace ui {

struct Bookmark {
    std::string name;
    math::Vec3 position;
    float yaw;
    float pitch;
    graphics::CameraMode mode;
};

} // namespace ui
