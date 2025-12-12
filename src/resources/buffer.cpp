#include "buffer.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace resources {

Buffer::Buffer(const core::GraphicsContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : device_(context.device()), size_(size) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    if (vkBindBufferMemory(device_, buffer_, memory_, 0) != VK_SUCCESS) {
        throw std::runtime_error("failed to bind buffer memory!");
    }
}

Buffer::~Buffer() {
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
}

Buffer::Buffer(Buffer&& other) noexcept 
    : device_(other.device_), buffer_(other.buffer_), memory_(other.memory_), size_(other.size_), mapped_(other.mapped_) {
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.mapped_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (buffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, buffer_, nullptr);
        if (memory_ != VK_NULL_HANDLE) vkFreeMemory(device_, memory_, nullptr);
        
        device_ = other.device_;
        buffer_ = other.buffer_;
        memory_ = other.memory_;
        size_ = other.size_;
        mapped_ = other.mapped_;

        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.mapped_ = nullptr;
    }
    return *this;
}

void* Buffer::map() {
    if (mapped_) return mapped_;
    vkMapMemory(device_, memory_, 0, size_, 0, &mapped_);
    return mapped_;
}

void Buffer::unmap() {
    if (mapped_) {
        vkUnmapMemory(device_, memory_);
        mapped_ = nullptr;
    }
}

void Buffer::upload(const void* data, VkDeviceSize size) {
    void* ptr = map();
    std::memcpy(ptr, data, static_cast<size_t>(size));
    unmap();
}

uint32_t Buffer::findMemoryType(const core::GraphicsContext& context, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

} // namespace resources
