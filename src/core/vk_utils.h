#pragma once

#include <vulkan/vulkan.h>
#include "../log.h"
#include <cstdio>

namespace core {

inline void check_vk(VkResult err) {
    if (err != VK_SUCCESS) {
        logMessage(LogLevel::Error, "Vulkan Error: %d\n", err);
    }
}

} // namespace core
