#include "mesh.h"
#include <iostream>
#include "../core/command_pool.h"

namespace graphics {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

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

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, uv);

    return attributeDescriptions;
}


Mesh::Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
    : indexCount_(static_cast<uint32_t>(indices.size())), indexType_(VK_INDEX_TYPE_UINT16) {
    
    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = createDeviceLocalBuffer(context, vertices.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VkDeviceSize indexSize = sizeof(uint16_t) * indices.size();
    indexBuffer_ = createDeviceLocalBuffer(context, indices.data(), indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

Mesh::Mesh(const core::GraphicsContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : indexCount_(static_cast<uint32_t>(indices.size())), indexType_(VK_INDEX_TYPE_UINT32) {

    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = createDeviceLocalBuffer(context, vertices.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
    indexBuffer_ = createDeviceLocalBuffer(context, indices.data(), indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

std::unique_ptr<resources::Buffer> Mesh::createDeviceLocalBuffer(
    const core::GraphicsContext& context, 
    const void* data, 
    VkDeviceSize size, 
    VkBufferUsageFlags usage) 
{
    // 1. Staging Buffer (Host Visible)
    resources::Buffer staging(
        context, size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    staging.upload(data, size);

    // 2. Device Buffer (Device Local)
    auto deviceBuffer = std::make_unique<resources::Buffer>(
        context, size, 
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // 3. Copy Command
    core::CommandPool pool(context, context.queueFamilyIndex());
    VkCommandBuffer cmd = pool.allocate(1)[0];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, staging.handle(), deviceBuffer->handle(), 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(context.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.graphicsQueue());

    return deviceBuffer;
}

void Mesh::draw(VkCommandBuffer cmd) const {
    VkBuffer buffers[] = { vertexBuffer_->handle() }; // Fix access to handle()
    VkDeviceSize offsets[] = { 0 };
    
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, indexType_);
    
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

} // namespace graphics
