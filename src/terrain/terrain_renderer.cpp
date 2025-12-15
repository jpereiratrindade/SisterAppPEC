#include "terrain_renderer.h"
#include "../graphics/geometry_utils.h"
#include <iostream>

namespace shape {

TerrainRenderer::TerrainRenderer(const core::GraphicsContext& ctx, VkRenderPass renderPass) : ctx_(ctx) {
    // Reuse existing basic shader for v1 (Vertex Color)
    auto vs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.vert.spv");
    auto fs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.frag.spv");
    
    // Create Material with VALID RenderPass!
    material_ = std::make_unique<graphics::Material>(ctx_, renderPass, VkExtent2D{1280,720}, vs, fs, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, true, true); 
}
    // Material initialized.

void TerrainRenderer::buildMesh(const terrain::TerrainMap& map, float gridScale) {
    int w = map.getWidth();
    int h = map.getHeight();
    
    std::vector<graphics::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(w * h);
    indices.reserve((w - 1) * (h - 1) * 6);

    // 1. Generate Vertices
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            float height = map.getHeight(x, z);
            
            // Calculate Smooth Normal
            float hL = map.getHeight(x > 0 ? x-1 : x, z);
            float hR = map.getHeight(x < w-1 ? x+1 : x, z);
            float hD = map.getHeight(x, z > 0 ? z-1 : z);
            float hU = map.getHeight(x, z < h-1 ? z+1 : z);
            
            // Central differences
            float dx = (hR - hL); // run=2
            float dz = (hU - hD);
            // Normal = cross( (2, dx, 0), (0, dz, 2) ) -> normalized
            // Roughly (-dx, 2, -dz)
            
            float nx = -dx;
            float ny = 2.0f; // Scale factor affecting steepness
            float nz = -dz;
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            
            graphics::Vertex v{};
            v.pos[0] = static_cast<float>(x) * gridScale;
            v.pos[1] = height;
            v.pos[2] = static_cast<float>(z) * gridScale;
            
            v.normal[0] = nx / len;
            v.normal[1] = ny / len;
            v.normal[2] = nz / len;

            // Visualization Colors
            // Slope-based coloring
            float slope = 1.0f - v.normal[1]; // 0 = flat, 1 = vertical
            
            if (slope < 0.15f) { // Flat (Soil/Dirt)
                v.color[0] = 0.55f; v.color[1] = 0.47f; v.color[2] = 0.36f; // Light Brown
            } else if (slope < 0.4f) { // Hill (Darker Soil/Rock mix)
                v.color[0] = 0.45f; v.color[1] = 0.38f; v.color[2] = 0.31f; // Darker Brown
            } else { // Cliff (Rock)
                v.color[0] = 0.4f; v.color[1] = 0.4f; v.color[2] = 0.45f; // Blue-Grey Rock
            }
            
            // v3.6.1 Flux (Drainage) Visualization
            // Store flux in UV.x for shader-based visualization toggling.
            float flux = map.fluxMap()[z * w + x];
            v.uv[0] = flux;
            
            // v3.6.2 Erosion (Sediment) Visualization
            // Store sediment in UV.y
            float sed = map.sedimentMap()[z * w + x];
            v.uv[1] = sed;

            // v3.6.3 Watershed Visualization
            // Store Basin ID in auxiliary
            v.auxiliary = static_cast<float>(map.watershedMap()[z * w + x]);
            
            vertices.push_back(v);
        }
    }

    // Indices (same as before)
    for (int z = 0; z < h - 1; ++z) {
        for (int x = 0; x < w - 1; ++x) {
            int topLeft = z * w + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * w + x;
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    mesh_ = std::make_unique<graphics::Mesh>(ctx_, vertices, indices);
    
    // Debug Flux
    float maxFlux = 0.0f;
    for (const auto& v : vertices) maxFlux = std::max(maxFlux, v.uv[0]);
    std::cout << "[TerrainRenderer] Mesh Built: " << vertices.size() << " verts. Max Flux: " << maxFlux << std::endl;
}

void TerrainRenderer::render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, bool showSlopeVis, bool showDrainageVis, float drainageIntensity, bool showWatershedVis, bool showBasinOutlines) {
    if (!mesh_ || !material_) return;
    
    // Bind Pipeline and Descriptor Sets
    material_->bind(cmd);

    // Define PC struct matching shader
    // Unified PushConstants Layout (Aligned)
    struct PushConstants {
        float mvp[16];          // 0
        float pointSize;        // 64
        float useLighting;      // 68
        float useFixedColor;    // 72
        float opacity;          // 76
        float fixedColor[4];    // 80 (vec4)
        float useSlopeVis;      // 96
        float drainageIntensity;// 100 [NEW]
        float useBasinOutlines; // 104 [NEW] Match Shader!
        float padding;          // 108 (align next vec4)
        float cameraPos[4];     // 112 (vec4)
        float useDrainageVis;   // 128
        float useErosionVis;    // 132
        float useWatershedVis;  // 136 [NEW]
    };

    PushConstants pc{};
    std::copy(mvp.begin(), mvp.end(), pc.mvp);
    pc.pointSize = 1.0f;
    pc.useLighting = 1.0f; 
    pc.useFixedColor = 0.0f;
    pc.opacity = 1.0f;
    pc.fixedColor[0] = 0.0f; pc.fixedColor[1] = 0.0f; pc.fixedColor[2] = 0.0f; pc.fixedColor[3] = 1.0f;
    pc.useSlopeVis = showSlopeVis ? 1.0f : 0.0f;
    pc.drainageIntensity = drainageIntensity; // [NEW]
    pc.useBasinOutlines = showBasinOutlines ? 1.0f : 0.0f; // [NEW] Correct Offset
    pc.cameraPos[0] = 0.0f; pc.cameraPos[1] = 0.0f; pc.cameraPos[2] = 0.0f; pc.cameraPos[3] = 1.0f;
    pc.useDrainageVis = showDrainageVis ? 1.0f : 0.0f;
    pc.useErosionVis = 0.0f;
    pc.useWatershedVis = showWatershedVis ? 1.0f : 0.0f;

    vkCmdPushConstants(cmd, material_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    // Draw using Mesh API
    mesh_->draw(cmd);
}

} // namespace shape
