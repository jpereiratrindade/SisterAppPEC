#pragma once

#include "../core/graphics_context.h"
#include "../terrain/terrain_map.h" // For TerrainMap and TerrainConfig
#include "../graphics/camera.h"     // For Camera
#include <vulkan/vulkan.h>
#include <vector>

namespace ui {

class Minimap {
public:
    Minimap(const core::GraphicsContext& context);
    ~Minimap();

    // Updates the minimap texture from the terrain/soil data.
    // Should be called only when terrain changes.
    void update(const terrain::TerrainMap& map, const terrain::TerrainConfig& config);

    // Draws the minimap window using ImGui.
    // Handles Zoom and Pan internally.
    // Updates Camera position on click.
    void render(graphics::Camera& camera);

    // Get the ImGui Texture ID (VkDescriptorSet)
    VkDescriptorSet getTextureID() const { return textureID_; }

private:
    const core::GraphicsContext& context_;

    // Dimensions of the texture (Fixed to 512x512 for UI)
    uint32_t textureWidth_ = 512;
    uint32_t textureHeight_ = 512;
    
    // World Dimensions for Navigation
    float worldWidth_ = 1000.0f;
    float worldHeight_ = 1000.0f;

    // Vulkan Resources
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorSet textureID_ = VK_NULL_HANDLE;

    // State for Zoom/Pan
    float zoomLevel_ = 1.0f; // 1.0 = Full Map
    // Offset center normalized [0,1]
    float centerX_ = 0.5f; 
    float centerY_ = 0.5f;

    // Helpers
    void createResources();
    void destroyResources();
    void createSampler();
    
    // Command Buffer Helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace ui
