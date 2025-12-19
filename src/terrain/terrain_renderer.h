#pragma once
#include "terrain_map.h"
#include <vulkan/vulkan.h>
#include "../core/graphics_context.h"
#include "../graphics/material.h"
#include "../graphics/mesh.h"
#include <memory>
#include <vector>
#include "../vegetation/vegetation_types.h"

namespace ml { class MLService; }

namespace shape {

class TerrainRenderer {
public:
    TerrainRenderer(const core::GraphicsContext& ctx, VkRenderPass renderPass, VkCommandPool commandPool);
    ~TerrainRenderer() = default;

    struct MeshData {
        std::vector<graphics::Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    /**
     * @brief Build the GPU mesh from the terrain map.
     * Use generateMeshData + uploadMesh for async loading.
     */
    void buildMesh(const terrain::TerrainMap& map, float gridScale = 1.0f, const ml::MLService* mlService = nullptr);

    // Async Support
    static MeshData generateMeshData(const terrain::TerrainMap& map, float gridScale = 1.0f, const ml::MLService* mlService = nullptr);
    void uploadMesh(const MeshData& data);

    /**
     * @brief Record draw commands
     */
    void render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, 
                bool showSlopeVis, bool showDrainageVis, float drainageIntensity, 
                bool showWatershedVis, bool showBasinOutlines, bool showSoilVis,
                // Soil Whitelist
                bool soilHidroAllowed, bool soilBTextAllowed, bool soilArgilaAllowed, 
                bool soilBemDesAllowed, bool soilRasoAllowed, bool soilRochaAllowed,
                float sunAzimuth, float sunElevation, float fogDensity, float lightIntensity,
                float uvScale, int vegetationMode);

    // v3.9.0 Vegetation Support
    void createVegetationResources(int width, int height);
    void updateVegetation(const vegetation::VegetationGrid& grid);
    void destroyVegetationResources();

private:
    const core::GraphicsContext& ctx_;
    VkCommandPool commandPool_; // v3.9.0: Needed for texture uploads
    std::unique_ptr<graphics::Mesh> mesh_;
    std::unique_ptr<graphics::Material> material_;
    
    // Vegetation Texture Resources
    VkImage vegImage_ = VK_NULL_HANDLE;
    VkDeviceMemory vegMemory_ = VK_NULL_HANDLE;
    VkImageView vegView_ = VK_NULL_HANDLE;
    VkSampler vegSampler_ = VK_NULL_HANDLE;
    
    // v3.9.2: Async Upload Synchronization
    VkFence uploadFence_ = VK_NULL_HANDLE;
    VkCommandBuffer uploadCmd_ = VK_NULL_HANDLE;
    bool hasPendingUpload_ = false;

    // Persistent Staging Buffer
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    VkDeviceSize stagingSize_ = 0;
    
    // Vegetation Descriptors
    VkDescriptorSetLayout vegDescLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool vegDescPool_ = VK_NULL_HANDLE;
    VkDescriptorSet vegDescSet_ = VK_NULL_HANDLE;
    
    // v3.9.0: Track dimensions for resizing
    int vegWidth_ = 0;
    int vegHeight_ = 0;

    // Helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
};

} // namespace shape
