#pragma once
#include "terrain_map.h"
#include <vulkan/vulkan.h>
#include "../core/graphics_context.h"
#include "../graphics/material.h"
#include "../graphics/mesh.h"
#include <memory>
#include <vector>

namespace shape {

class TerrainRenderer {
public:
    TerrainRenderer(const core::GraphicsContext& ctx, VkRenderPass renderPass);
    ~TerrainRenderer() = default;

    /**
     * @brief Build the GPU mesh from the terrain map.
     * This is a "heavy" operation for large maps.
     */
    void buildMesh(const terrain::TerrainMap& map);

    /**
     * @brief Record draw commands
     */
    void render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, bool showSlopeVis = false, bool showDrainageVis = false);

private:
    const core::GraphicsContext& ctx_;
    std::unique_ptr<graphics::Mesh> mesh_;
    std::unique_ptr<graphics::Material> material_;
};

} // namespace shape
