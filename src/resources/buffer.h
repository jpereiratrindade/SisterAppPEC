#pragma once

#include <vulkan/vulkan.h>
#include "../core/graphics_context.h"

namespace resources {

/**
 * @brief RAII wrapper for VkBuffer and VkDeviceMemory.
 * 
 * Simplifies buffer creation by:
 * - Automatically allocating and binding memory
 * - Providing a simple upload() interface for data transfer
 * - Ensuring proper cleanup on destruction
 * 
 * Supports both host-visible (CPU accessible) and device-local (GPU only) memory.
 */
class Buffer {
public:
    /**
     * @brief Creates a Vulkan buffer with memory.
     * @param context Graphics context for device/physical device access
     * @param size Size in bytes
     * @param usage Buffer usage flags (VERTEX_BUFFER, INDEX_BUFFER, etc.)
     * @param properties Memory properties (HOST_VISIBLE, DEVICE_LOCAL, etc.)
     */
    Buffer(const core::GraphicsContext& context, VkDeviceSize size, 
           VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~Buffer();

    // No copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Movable
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void* map();
    void unmap();
    void upload(const void* data, VkDeviceSize size);

    VkBuffer handle() const { return buffer_; }
    VkDeviceMemory memory() const { return memory_; }
    VkDeviceSize size() const { return size_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    void* mapped_ = nullptr;

    uint32_t findMemoryType(const core::GraphicsContext& context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace resources
