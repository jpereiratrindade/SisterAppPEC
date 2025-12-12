#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace core {

class GraphicsContext;

/**
 * @brief Manages Vulkan synchronization primitives (Semaphores and Fences).
 * 
 * Each frame requires:
 * 1. ImageAvailable Semaphore (signaled when swapchain image is acquired)
 * 2. RenderFinished Semaphore (signaled when rendering is done)
 * 3. InFlight Fence (signaled when command buffer execution controls)
 * 
 * This class creates vectors of these objects based on the maximum number of frames in flight.
 */
class SyncObjects {
public:
    /**
     * @brief Creates synchronization objects.
     * @param ctx The graphics context.
     * @param maxFrames The number of frames in flight (usually matches swapchain image count).
     */
    SyncObjects(const GraphicsContext& ctx, uint32_t maxFrames);
    
    /**
     * @brief Destroys all semaphores and fences.
     */
    ~SyncObjects();

    // Non-copyable
    SyncObjects(const SyncObjects&) = delete;
    SyncObjects& operator=(const SyncObjects&) = delete;

    VkSemaphore imageAvailable(size_t index) const { return imageAvailable_[index]; }
    VkSemaphore renderFinished(size_t index) const { return renderFinished_[index]; }
    VkFence inFlight(size_t index) const { return inFlight_[index]; }
    const std::vector<VkFence>& inFlightFences() const { return inFlight_; }

    /**
     * @brief Waits for the fence at the given index to be signaled.
     * @param index Frame index.
     * @param timeoutNs Timeout in nanoseconds (default: UINT64_MAX).
     */
    void waitForFence(size_t index, uint64_t timeoutNs = UINT64_MAX);
    
    /**
     * @brief Resets the fence at the given index to the unsignaled state.
     * @param index Frame index.
     */
    void resetFence(size_t index);

private:
    const GraphicsContext& ctx_;
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence> inFlight_;
};

} // namespace core
