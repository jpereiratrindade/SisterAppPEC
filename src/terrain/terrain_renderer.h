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

    struct MeshData {
        std::vector<graphics::Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    /**
     * @brief Build the GPU mesh from the terrain map.
     * Use generateMeshData + uploadMesh for async loading.
     */
    void buildMesh(const terrain::TerrainMap& map, float gridScale = 1.0f);

    // Async Support
    static MeshData generateMeshData(const terrain::TerrainMap& map, float gridScale = 1.0f);
    void uploadMesh(const MeshData& data);

    /**
     * @brief Record draw commands
     */
    void render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, 
                bool showSlopeVis, bool showDrainageVis, float drainageIntensity, 
                bool showWatershedVis, bool showBasinOutlines, bool showSoilVis,
                // Soil Whitelist
                bool soilHidroAllowed, bool soilBTextAllowed, bool soilArgilaAllowed, 
                bool soilBemDesAllowed, bool soilRasoAllowed, bool soilRochaAllowed,
                float sunAzimuth, float sunElevation, float fogDensity, float lightIntensity);

private:
    const core::GraphicsContext& ctx_;
    std::unique_ptr<graphics::Mesh> mesh_;
    std::unique_ptr<graphics::Material> material_;
    // No change to material
};

} // namespace shape
