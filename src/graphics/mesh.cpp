#include "mesh.h"
#include <iostream>

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


Mesh::Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
    : indexCount_(static_cast<uint32_t>(indices.size())), indexType_(VK_INDEX_TYPE_UINT16) {
    
    // Vertex Buffer
    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = std::make_unique<resources::Buffer>(
        context, vertexSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer_->upload(vertices.data(), vertexSize);

    // Index Buffer (u16)
    VkDeviceSize indexSize = sizeof(uint16_t) * indices.size();
    indexBuffer_ = std::make_unique<resources::Buffer>(
        context, indexSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer_->upload(indices.data(), indexSize);
}

Mesh::Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : indexCount_(static_cast<uint32_t>(indices.size())), indexType_(VK_INDEX_TYPE_UINT32) {
    std::cout << "[Mesh] Creating Mesh with 32-bit indices. count=" << indexCount_ << std::endl;

    // Vertex Buffer
    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = std::make_unique<resources::Buffer>(
        context, vertexSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer_->upload(vertices.data(), vertexSize);

    // Index Buffer (u32)
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
    indexBuffer_ = std::make_unique<resources::Buffer>(
        context, indexSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer_->upload(indices.data(), indexSize);
}

void Mesh::draw(VkCommandBuffer cmd) const {
    VkBuffer buffers[] = { vertexBuffer_->handle() }; // Fix access to handle()
    VkDeviceSize offsets[] = { 0 };
    
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, indexType_);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

} // namespace graphics
