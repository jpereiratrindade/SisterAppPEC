#include "terrain_renderer.h"
#include "../graphics/geometry_utils.h"
#include <iostream>
#include <cstring>
#include <cmath>

namespace shape {

TerrainRenderer::TerrainRenderer(const core::GraphicsContext& ctx, VkRenderPass renderPass) : ctx_(ctx) {
    // Reuse existing basic shader for v1 (Vertex Color)
    auto vs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.vert.spv");
    auto fs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.frag.spv");
    
    // Create Material with VALID RenderPass!
    material_ = std::make_unique<graphics::Material>(ctx_, renderPass, VkExtent2D{1280,720}, vs, fs, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, true, true); 
}
    // Material initialized.

// 1. Refactored buildMesh to use helper
void TerrainRenderer::buildMesh(const terrain::TerrainMap& map, float gridScale) {
    MeshData data = generateMeshData(map, gridScale);
    uploadMesh(data);
}

void TerrainRenderer::uploadMesh(const MeshData& data) {
    // This MUST run on Main Thread (GPU Access)
    mesh_ = std::make_unique<graphics::Mesh>(ctx_, data.vertices, data.indices);
}

TerrainRenderer::MeshData TerrainRenderer::generateMeshData(const terrain::TerrainMap& map, float gridScale) {
    int w = map.getWidth();
    int h = map.getHeight();
    
    MeshData data;
    data.vertices.reserve(w * h);
    data.indices.reserve((w - 1) * (h - 1) * 6);

    std::vector<graphics::Vertex>& vertices = data.vertices;
    std::vector<uint32_t>& indices = data.indices;

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
            float ny = 2.0f * gridScale; // Correctly scale run by gridScale
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
            
            // v3.7.3 Semantic Soil ID
            v.soilId = static_cast<float>(map.soilMap()[z * w + x]);
            
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

    return data;
}

void TerrainRenderer::render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, 
                             bool showSlopeVis, bool showDrainageVis, float drainageIntensity, 
                             bool showWatershedVis, bool showBasinOutlines, bool showSoilVis,
                             bool soilHidroAllowed, bool soilBTextAllowed, bool soilArgilaAllowed, 
                             bool soilBemDesAllowed, bool soilRasoAllowed, bool soilRochaAllowed,
                             float sunAzimuth, float sunElevation, float fogDensity, float lightIntensity) {
    if (!mesh_ || !material_) return;
    
    // Bind Pipeline and Descriptor Sets
    material_->bind(cmd);

    // Define PC struct matching shader
    // Unified PushConstants Layout (Aligned)
    struct PushConstantsPacked {
        float mvp[16];
        float sunDir[4];
        float fixedColor[4];
        float params[4]; // x=opacity, y=drainageIntensity, z=fogDensity, w=pointSize
        uint32_t flags;  // Bitmask: 1=Lit, 2=FixCol, 4=Slope, 8=Drain, 16=Eros, 32=Water, 64=Soil, 128=Basin
        float pad[3];
    } pc;
    static_assert(sizeof(pc) == 128, "PC size mismatch");

    std::memcpy(pc.mvp, mvp.data(), sizeof(float) * 16);
    
    // SunDir (Normalize here)
    float toRad = 3.14159265f / 180.0f;
    float radAz = sunAzimuth * toRad;
    float radEl = sunElevation * toRad;
    pc.sunDir[0] = std::cos(radEl) * std::sin(radAz);
    pc.sunDir[1] = std::sin(radEl);
    pc.sunDir[2] = std::cos(radEl) * std::cos(radAz);
    pc.sunDir[3] = 0.0f;

    pc.fixedColor[0] = 1.0f; pc.fixedColor[1] = 1.0f; pc.fixedColor[2] = 1.0f; pc.fixedColor[3] = 1.0f;
    
    pc.params[0] = 1.0f; // Opacity
    pc.params[1] = drainageIntensity;
    pc.params[2] = fogDensity;
    pc.params[3] = lightIntensity; // v3.8.1 Light Intensity

    // Pack Flags
    int mask = 0;
    if (showSlopeVis || showSoilVis) { /* Lighting logic? User says FORCE it. */ mask |= 1; } // Use Lighting Always?
    // Wait, step 1258 says "pc.useLighting = 1.0f" ALWAYS.
    mask |= 1; // lighting default ON
    
    if (false) mask |= 2; // FixedColor unused currently or 0
    if (showSlopeVis) mask |= 4;
    if (showDrainageVis) mask |= 8;
    // Erosion? Internal/Deprecated, let's keep slot: mask |= 16;
    if (showWatershedVis) mask |= 32;
    if (showSoilVis) mask |= 64;
    if (showBasinOutlines) mask |= 128;
    
    // Soil Whitelist Bits (8-13)
    if (soilHidroAllowed)  mask |= 256;
    if (soilBTextAllowed)  mask |= 512;
    if (soilArgilaAllowed) mask |= 1024;
    if (soilBemDesAllowed) mask |= 2048;
    if (soilRasoAllowed)   mask |= 4096;
    if (soilRochaAllowed)  mask |= 8192;
    
    pc.flags = static_cast<uint32_t>(mask);

    vkCmdPushConstants(cmd, material_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Draw using Mesh API
    mesh_->draw(cmd);
}

} // namespace shape
