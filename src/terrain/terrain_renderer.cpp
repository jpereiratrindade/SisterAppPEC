#include "terrain_renderer.h"
#include "../graphics/geometry_utils.h"
#include <iostream>

namespace shape {

TerrainRenderer::TerrainRenderer(const core::GraphicsContext& ctx) : ctx_(ctx) {
    // Reuse existing basic shader for v1 (Vertex Color)
    auto vs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.vert.spv");
    auto fs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.frag.spv");
    material_ = std::make_unique<graphics::Material>(ctx_, VK_NULL_HANDLE, VkExtent2D{1280,720}, vs, fs); 
    // Note: RenderPass and Extent will need to be dynamic or updated, simplified constr here for prototype
    // Actually Material needs a valid RenderPass to create pipeline.
    // Ideally we pass the render pass from Application into init or build.
    // For now we will defer material creation or accept it's broken until linked with Application.
    // Let's assume we pass renderPass later or fix this API. 
    // FIX: Just store shaders for now and init material properly when needed? 
    // No, let's assume we use the existing material system.
}

void TerrainRenderer::buildMesh(const terrain::TerrainMap& map) {
    int w = map.getWidth();
    int h = map.getHeight();
    
    std::vector<graphics::Vertex> vertices;
    std::vector<uint16_t> indices;
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
            v.pos[0] = static_cast<float>(x);
            v.pos[1] = height;
            v.pos[2] = static_cast<float>(z);
            
            v.normal[0] = nx / len;
            v.normal[1] = ny / len;
            v.normal[2] = nz / len;

            // Visualization Colors
            // Slope-based coloring
            float slope = 1.0f - v.normal[1]; // 0 = flat, 1 = vertical
            
            if (slope < 0.1f) { // Flat
                v.color[0] = 0.2f; v.color[1] = 0.6f; v.color[2] = 0.2f; // Green
            } else if (slope < 0.3f) { // Hill
                v.color[0] = 0.4f; v.color[1] = 0.5f; v.color[2] = 0.2f; // Olive
            } else { // Cliff
                v.color[0] = 0.5f; v.color[1] = 0.45f; v.color[2] = 0.4f; // Grey/Brown
            }
            
            // Sediment Visualization (Red tint)
            float sed = map.sedimentMap()[z * w + x];
            if (sed > 0.1f) {
                v.color[0] += std::min(sed * 0.5f, 0.5f);
                v.color[1] *= 0.5f;
                v.color[2] *= 0.5f;
            }

            vertices.push_back(v);
        }
    }

    // 2. Generate Indices (Triangle Strip or List)
    for (int z = 0; z < h - 1; ++z) {
        for (int x = 0; x < w - 1; ++x) {
            uint16_t topLeft = static_cast<uint16_t>((z + 1) * w + x);
            uint16_t topRight = static_cast<uint16_t>((z + 1) * w + (x + 1));
            uint16_t bottomLeft = static_cast<uint16_t>(z * w + x);
            uint16_t bottomRight = static_cast<uint16_t>(z * w + (x + 1));

            // Triangle 1
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            indices.push_back(topLeft);
            
            // Triangle 2
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            indices.push_back(topRight);
        }
    }

    mesh_ = std::make_unique<graphics::Mesh>(ctx_, vertices, indices);
    std::cout << "[TerrainRenderer] Mesh Built: " << vertices.size() << " verts, " << indices.size() / 3 << " tris." << std::endl;
}

void TerrainRenderer::render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport) {
    if (!mesh_ || !material_) return;
    
    // Bind Pipeline and Descriptor Sets
    material_->bind(cmd);

    // Define PC struct matching shader
    struct PushConstants {
        std::array<float, 16> mvp;
        float pointSize;
        float useLighting;
        float useFixedColor;
        float opacity;
        float pad[3]; // vec3 alignment padding if needed, but here packed tight? 
        // vec3 fixedColor (12 bytes)
        float r, g, b;
    };

    PushConstants pc{};
    pc.mvp = mvp;
    pc.pointSize = 1.0f;
    pc.useLighting = 1.0f; // Enable lighting
    pc.useFixedColor = 0.0f;
    pc.opacity = 1.0f; // Visible!
    pc.r = 0.0f; pc.g = 0.0f; pc.b = 0.0f;

    // Push Constants (MVP + Params)
    vkCmdPushConstants(cmd, material_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Draw using Mesh API
    mesh_->draw(cmd);
}

} // namespace shape
