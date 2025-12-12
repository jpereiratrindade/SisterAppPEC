#pragma once


#include <vulkan/vulkan.h>

namespace graphics {

class Mesh;
class Material;

struct RenderOptions {
    float pointSize = 1.0f;
    bool useLighting = false;
    bool useFixedColor = false;
    float fixedColor[3] = {1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    float cameraPos[3] = {0.0f, 0.0f, 0.0f};
};

// Push Constants Layout (Shared)
struct RendererPushConstants {
    float mvp[16];
    float pointSize;
    float useLighting;
    float useFixedColor;
    float opacity;
    float fixedColor[3];
    float padding; // Align cameraPos to 16 bytes (offset 96)
    float cameraPos[3];
};

class Renderer {
public:
    // Renderer is stateless regarding device/swapchain now.
    // Init/Describe kept for API interface consistency if needed, 
    // but they no longer need the context.
    bool init() { return true; }
    void destroy() {}

    void bindPipeline(VkCommandBuffer cmd, const Material* material, VkExtent2D extent) const;
    void record(VkCommandBuffer cmd, const Mesh* mesh, const Material* material, VkExtent2D extent, const float* mvp4x4, const RenderOptions& options = {}) const;
};

} // namespace graphics
