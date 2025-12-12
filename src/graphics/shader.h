#pragma once

#include <vulkan/vulkan.h>
#include "../core/graphics_context.h"
#include <string>

namespace graphics {

/**
 * @brief RAII wrapper for VkShaderModule with smart file loading.
 * 
 * Loads SPIR-V shader binaries from disk with automatic path resolution:
 * - Tries the provided path first
 * - Falls back to build/shaders/, ../shaders/, etc.
 * 
 * Shader stage is automatically detected from filename extension:
 * - .vert.spv -> Vertex shader
 * - .frag.spv -> Fragment shader
 * - .comp.spv -> Compute shader
 */
class Shader {
public:
    /**
     * @brief Loads a SPIR-V shader module from file.
     * @param context Graphics context for device access
     * @param filepath Path to .spv file (relative or absolute)
     * @throws std::runtime_error if file not found or shader creation fails
     */
    Shader(const core::GraphicsContext& context, const std::string& filepath);
    ~Shader();

    // No copy
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Movable
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    VkShaderModule handle() const { return module_; }
    VkShaderStageFlagBits stage() const; // Helper to guess stage from filename

private:
    VkDevice device_;
    VkShaderModule module_ = VK_NULL_HANDLE;
    std::string filepath_;
};

} // namespace graphics
