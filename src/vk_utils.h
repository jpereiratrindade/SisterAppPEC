#pragma once

#include <vulkan/vulkan.h>

#include "log.h"

inline bool vk_ok(VkResult res, const char* what) {
    if (res != VK_SUCCESS) {
        logMessage(LogLevel::Error, "%s falhou (VkResult=%d)\n", what, res);
        return false;
    }
    return true;
}
