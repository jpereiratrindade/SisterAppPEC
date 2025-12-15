#pragma once

#include "../resources/buffer.h"
#include <vector>
#include <array>
#include <memory>

namespace graphics {

/**
 * @brief Vertex format for 3D geometry.
 * 
 * Layout:
 * - Location 0: vec3 position
 * - Location 1: vec3 color
 */
struct Vertex {
    float pos[3];    ///< Vertex position in model space
    float color[3];  ///< RGB vertex color (0.0 - 1.0)
    float normal[3]; ///< Vertex normal for lighting

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

/**
 * @brief Represents renderable 3D geometry.
 * 
 * Owns vertex and index buffers and provides a simple draw() interface.
 * Meshes can be shared across multiple entities and materials.
 */
class Mesh {
public:
    /**
     * @brief Creates a mesh from vertex and index data.
     * @param context Graphics context for buffer creation
     * @param vertices Vertex data (position + color)
     * @param indices Triangle indices (CCW winding)
     */
    Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
    Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices); // u32 overload
    
    // Non-copyable/moveable for now simplicty
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw(VkCommandBuffer cmd) const;

private:
    std::unique_ptr<resources::Buffer> vertexBuffer_;
    std::unique_ptr<resources::Buffer> indexBuffer_;
    uint32_t indexCount_ = 0;
    VkIndexType indexType_ = VK_INDEX_TYPE_UINT16;
};

} // namespace graphics
