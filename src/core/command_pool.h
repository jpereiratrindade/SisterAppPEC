#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace core {

class GraphicsContext;

/**
 * @brief RAII wrapper for a Vulkan Command Pool.
 * 
 * Manages the lifecycle of a VkCommandPool. Also provides utility methods
 * to allocate and free command buffers from this pool.
 */
class CommandPool {
public:
    /**
     * @brief Creates a command pool.
     * @param ctx The graphics context.
     * @param queueFamilyIndex The queue family index this pool will allocate buffers for.
     */
    CommandPool(const GraphicsContext& ctx, uint32_t queueFamilyIndex);
    
    /**
     * @brief Destroys the command pool and implicitly frees all buffers allocated from it.
     */
    ~CommandPool();

    // Non-copyable
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    /**
     * @brief Allocates primary command buffers.
     * @param count The number of buffers to allocate.
     * @return A vector of allocated command buffers.
     */
    std::vector<VkCommandBuffer> allocate(uint32_t count);
    
    /**
     * @brief Frees specific command buffers.
     * @param buffers The buffers to free.
     */
    void free(const std::vector<VkCommandBuffer>& buffers);

    VkCommandPool handle() const { return pool_; }

private:
    const GraphicsContext& ctx_;
    VkCommandPool pool_ = VK_NULL_HANDLE;
};

} // namespace core
