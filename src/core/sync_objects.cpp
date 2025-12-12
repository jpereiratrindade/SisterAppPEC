#include "sync_objects.h"
#include "graphics_context.h"
#include <stdexcept>
#include <limits>

namespace core {

SyncObjects::SyncObjects(const GraphicsContext& ctx, uint32_t maxFrames)
    : ctx_(ctx) {
    imageAvailable_.resize(maxFrames);
    renderFinished_.resize(maxFrames);
    inFlight_.resize(maxFrames);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFrames; i++) {
        if (vkCreateSemaphore(ctx_.device(), &semaphoreInfo, nullptr, &imageAvailable_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(ctx_.device(), &semaphoreInfo, nullptr, &renderFinished_[i]) != VK_SUCCESS ||
            vkCreateFence(ctx_.device(), &fenceInfo, nullptr, &inFlight_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

SyncObjects::~SyncObjects() {
    VkDevice device = ctx_.device();
    for (auto s : imageAvailable_) vkDestroySemaphore(device, s, nullptr);
    for (auto s : renderFinished_) vkDestroySemaphore(device, s, nullptr);
    for (auto f : inFlight_) vkDestroyFence(device, f, nullptr);
}

void SyncObjects::waitForFence(size_t index, uint64_t timeoutNs) {
    if (vkWaitForFences(ctx_.device(), 1, &inFlight_[index], VK_TRUE, timeoutNs) != VK_SUCCESS) {
        // Handle timeout or error if necessary, for now maybe just log or ignore? 
        // The original logic didn't handle it explicitly other than waiting UINT64_MAX.
    }
}

void SyncObjects::resetFence(size_t index) {
    vkResetFences(ctx_.device(), 1, &inFlight_[index]);
}

} // namespace core
