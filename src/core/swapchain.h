#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <SDL2/SDL.h>

namespace core {

class GraphicsContext;

/**
 * @brief Manages the Vulkan Swapchain and related presentation resources.
 * 
 * This class encapsulates:
 * - VkSwapchainKHR
 * - VkImage and VkImageView for the swapchain
 * - Depth buffer resources (Image, Memory, View)
 * - The main RenderPass
 * - Framebuffers compatible with the RenderPass
 * 
 * It supports easy recreation (e.g., on window resize) via the `recreate()` method.
 */
class Swapchain {
public:
    /**
     * @brief Creates a swapchain for the given context and window.
     * @param ctx The active graphics context.
     * @param window The SDL window (used for size querying).
     */
    Swapchain(const GraphicsContext& ctx, SDL_Window* window, bool vsync = false);
    
    /**
     * @brief Destroys the swapchain and all associated resources (framebuffers, views, etc.).
     */
    ~Swapchain();

    // Non-copyable
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    /**
     * @brief Recreates the swapchain and all dependents.
     * 
     * Call this when the window is resized. It handles waiting for device idle,
     * destroying old resources, and creating new ones with the new window dimensions.
     * 
     * @param window The window to query new dimensions from.
     */
    void recreate(SDL_Window* window, bool vsync);

    // Getters
    VkSwapchainKHR handle() const { return swapchain_; }
    VkFormat imageFormat() const { return imageFormat_; }
    VkExtent2D extent() const { return extent_; }
    VkRenderPass renderPass() const { return renderPass_; }
    
    const std::vector<VkFramebuffer>& framebuffers() const { return framebuffers_; }
    const std::vector<VkImage>& images() const { return images_; }
    const std::vector<VkImageView>& imageViews() const { return imageViews_; }

private:
    void createSwapchain(SDL_Window* window, bool vsync);
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();

    void cleanup();

    const GraphicsContext& ctx_;
    
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;

    // Depth resources
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
};

} // namespace core
