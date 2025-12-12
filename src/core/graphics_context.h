#pragma once

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <memory>

namespace core {

/**
 * @brief Manages the fundamental Vulkan objects (Instance, Device, Surface).
 * 
 * This class applies RAII principles to the core Vulkan context. It handles:
 * - Instance creation (with optional validation layers)
 * - Physical device selection (prioritizing discrete GPUs)
 * - Logical device creation
 * - SDL2 Surface creation
 * 
 * All resources are destroyed automatically in the destructor.
 */
class GraphicsContext {
public:
    /**
     * @brief Initializes the Vulkan context.
     * 
     * @param window The SDL window to create a surface for.
     * @param enableValidation If true, enables standard validation layers.
     * @throws std::runtime_error If Vulkan initialization fails.
     */
    GraphicsContext(SDL_Window* window, bool enableValidation = false);
    
    /**
     * @brief Destructor. cleans up Device, Surface, Debug Messenger, and Instance.
     */
    ~GraphicsContext();

    // Context is non-copyable to prevent double-free.
    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;

    // Getters
    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkDevice device() const { return device_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkSurfaceKHR surface() const { return surface_; }
    uint32_t queueFamilyIndex() const { return queueFamilyIndex_; }
    bool supportsWireframe() const { return supportsWireframe_; }

private:
    void createInstance(bool enableValidation);
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    SDL_Window* window_; 
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;
    bool supportsWireframe_ = false;
};

} // namespace core
