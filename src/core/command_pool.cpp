#include "command_pool.h"
#include "graphics_context.h"
#include <stdexcept>

namespace core {

CommandPool::CommandPool(const GraphicsContext& ctx, uint32_t queueFamilyIndex)
    : ctx_(ctx) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(ctx_.device(), &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

CommandPool::~CommandPool() {
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx_.device(), pool_, nullptr);
    }
}

std::vector<VkCommandBuffer> CommandPool::allocate(uint32_t count) {
    std::vector<VkCommandBuffer> buffers(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(ctx_.device(), &allocInfo, buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    return buffers;
}

void CommandPool::free(const std::vector<VkCommandBuffer>& buffers) {
    if (!buffers.empty()) {
        vkFreeCommandBuffers(ctx_.device(), pool_, static_cast<uint32_t>(buffers.size()), buffers.data());
    }
}

} // namespace core
