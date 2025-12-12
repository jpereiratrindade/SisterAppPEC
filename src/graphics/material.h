#pragma once

#include <vulkan/vulkan.h>
#include "../core/graphics_context.h"
#include "shader.h"
#include <vector>
#include <memory>

namespace graphics {

/**
 * @brief Encapsulates a Vulkan graphics pipeline and its layout.
 * 
 * A Material defines how geometry is rendered by combining:
 * - Vertex and fragment shaders
 * - Pipeline state (rasterization, depth testing, blending)
 * - Vertex input configuration
 * - Push constant layout for MVP matrices
 * 
 * Multiple meshes can use the same Material for efficient rendering.
 */
class Material {
public:
    /**
     * @brief Creates a graphics pipeline from shaders.
     * @param context Graphics context for device access
     * @param renderPass Target render pass
     * @param extent Swapchain extent for viewport/scissor
     * @param vertShader Vertex shader module
     * @param fragShader Fragment shader module
     */
    Material(const core::GraphicsContext& context, VkRenderPass renderPass, VkExtent2D extent, 
             std::shared_ptr<Shader> vertShader, std::shared_ptr<Shader> fragShader,
             VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
             VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
             bool enableBlend = false,
             bool depthWrite = true);
    ~Material();

    // No copy
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    // Movable
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;

    void bind(VkCommandBuffer cmd) const;
    VkPipelineLayout layout() const { return pipelineLayout_; }
    VkPipeline pipeline() const { return pipeline_; }

private:
    void createPipeline(VkRenderPass renderPass, VkExtent2D extent, VkPrimitiveTopology topology, VkPolygonMode polygonMode, bool enableBlend, bool depthWrite);

    VkDevice device_;
    std::shared_ptr<Shader> vertShader_;
    std::shared_ptr<Shader> fragShader_;

    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace graphics
