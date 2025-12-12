#include "mesh.h"

namespace graphics {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, normal);

    return attributeDescriptions;
}

Mesh::Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) {
    indexCount_ = static_cast<uint32_t>(indices.size());

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    
    // Perform staging logic or host visible upload
    // For simplicity, we stick to HOST_VISIBLE | COHERENT for now as in the original code
    // Optimization: Use Staging Buffer + DEVICE_LOCAL in future step
    
    vertexBuffer_ = std::make_unique<resources::Buffer>(
        context, 
        bufferSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer_->upload(vertices.data(), bufferSize);

    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    indexBuffer_ = std::make_unique<resources::Buffer>(
        context,
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer_->upload(indices.data(), indexBufferSize);
}

void Mesh::draw(VkCommandBuffer cmd) const {
    VkBuffer buffers[] = { vertexBuffer_->handle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

} // namespace graphics
