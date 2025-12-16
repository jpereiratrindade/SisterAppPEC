#include "minimap.h"
#include "../resources/buffer.h"
#include "../core/command_pool.h"
#include "../terrain/terrain_renderer.h" // reuse soil colors if possible, or redefine?
// Let's redefine readable colors or include header. 
// TerrainRenderer has getSoilColor but it's not static.
// We'll reimplement simple coloring here for stability.

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace ui {

Minimap::Minimap(const core::GraphicsContext& context)
    : context_(context) {
    createResources();
}

Minimap::~Minimap() {
    destroyResources();
}

void Minimap::createResources() {
    // 1. Create Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = textureWidth_;
    imageInfo.extent.height = textureHeight_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(context_.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create minimap image!");
    }

    // 2. Allocate Memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context_.device(), image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(context_.device(), &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate minimap image memory!");
    }

    vkBindImageMemory(context_.device(), image_, imageMemory_, 0);

    // 3. Create View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context_.device(), &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create minimap image view!");
    }

    // 4. Create Sampler
    createSampler();

    // 5. Register with ImGui
    // Ensure Layout is Shader Read Only before adding? 
    // ImGui expects correct layout at draw time. We initialize it to Undefined, 
    // but update() will transition it.
    // However, AddTexture might validation check? Usually no.
    
    // We must transition to ShaderReadOnly at least once before use. 
    // Let's do a quick transition of empty image.
    transitionImageLayout(image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    textureID_ = ImGui_ImplVulkan_AddTexture(sampler_, imageView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Minimap::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(context_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create minimap sampler!");
    }
}

void Minimap::destroyResources() {
    VkDevice device = context_.device(); // This might crash if context is already dead? No, context is ref.
    // However, if device handle inside context is invalid? 
    // We assume context is alive.
    
    if (sampler_) {
        vkDestroySampler(device, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (imageView_) {
        vkDestroyImageView(device, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
    }
    if (image_) {
        vkDestroyImage(device, image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (imageMemory_) {
        vkFreeMemory(device, imageMemory_, nullptr);
        imageMemory_ = VK_NULL_HANDLE;
    }
}

void Minimap::update(const terrain::TerrainMap& map, const terrain::TerrainConfig& config) {
    if (image_ == VK_NULL_HANDLE) return;

    // Generate CPU Pixel Data
    // Downsample from Map (Width x Height) to Texture (512 x 512)
    std::vector<uint32_t> pixels(textureWidth_ * textureHeight_);

    int mapW = map.getWidth();
    int mapH = map.getHeight();
    worldWidth_ = mapW * config.resolution;
    worldHeight_ = mapH * config.resolution;

    // Helper to get color
    auto getColor = [&](int x, int z) -> uint32_t {
        terrain::SoilType type = map.getSoil(x, z);
        float h = map.getHeight(x, z);
        // Simple Hillshade
        float hL = map.getHeight(std::max(0, x-1), z);
        float hU = map.getHeight(x, std::min(mapH-1, z+1));
        float slopeX = h - hL;
        float slopeZ = hU - h;
        // v3.8.1: Boost contrast for Hills (was 0.3f)
        // With 1024 resolution, slopes are small per pixel. Multiply by larger factor.
        float light = 0.5f + 1.5f * (slopeX - slopeZ); 
        light = std::clamp(light, 0.4f, 1.3f); // Allow brighter highlights, deeper shadows

        uint8_t r = 100, g = 100, b = 100;
        switch(type) {
            case terrain::SoilType::Raso:          r=200; g=200; b=100; break; // Yellowish
            case terrain::SoilType::BemDes:        r=139; g=69;  b=19;  break; // SaddleBrown
            case terrain::SoilType::Hidromorfico:  r=100; g=100; b=100; break; // Gray/Dark
            case terrain::SoilType::Argila:        r=160; g=82;  b=45;  break; // Sienna
            case terrain::SoilType::BTextural:     r=205; g=133; b=63;  break; // Peru
            case terrain::SoilType::Rocha:         r=80;  g=80;  b=80;  break; // Dark Gray
            case terrain::SoilType::None:          r=255; g=0;   b=255; break; // Magenta (Debug)
            default: break;
        }
        
        // v3.8.1: Transparent Water on Minimap to show bathymetry/texture
        if (h < config.waterLevel) {
            // Blend with Blue (R=50, G=100, B=200)
            // Factor depends on depth? Let's just do fixed tint for visibility.
            r = (r + 50) / 2;
            g = (g + 100) / 2;
            b = (b + 200) / 2;
            // Keep light calc for relief
        }

        r = static_cast<uint8_t>(std::min(255.0f, r * light));
        g = static_cast<uint8_t>(std::min(255.0f, g * light));
        b = static_cast<uint8_t>(std::min(255.0f, b * light));
        
        return (255 << 24) | (b << 16) | (g << 8) | r; // RGBA packed (ImGui uses Standard ordering, ensure Endianness?)
        // Actually typically ABGR or RGBA. Standard is R G B A in byte order.
        // uint32: 0xAABBGGRR (Little Endian) -> R at lowest. 
    };

    #pragma omp parallel for collapse(2)
    for (uint32_t y = 0; y < textureHeight_; ++y) {
        for (uint32_t x = 0; x < textureWidth_; ++x) {
            // Map to Terrain Coords
            float u = static_cast<float>(x) / textureWidth_;
            float v = static_cast<float>(y) / textureHeight_;
            
            // Flip V if needed? Terrain 0,0 is usually bottom-left. 
            // Image 0,0 is Top-Left. 
            // So v should map to (1-v) for correct orientation if Z is Up.
            // Let's assume Z grows up. Map (0,0) is bottom-left.
            // Image (0,0) top-left.
            // So Image Y=0 -> World Z = Max. Image Y=Max -> World Z = 0.
            int mapX = static_cast<int>(u * mapW);
            int mapZ = static_cast<int>(v * mapH); // v3.8.0 Fix: V=0 is North (Z=0). No inversion.
            
            mapX = std::max(0, std::min(mapW - 1, mapX));
            mapZ = std::max(0, std::min(mapH - 1, mapZ));
            
            pixels[y * textureWidth_ + x] = getColor(mapX, mapZ);
        }
    }

    // Upload
    // 1. Staging Buffer
    VkDeviceSize imageSize = textureWidth_ * textureHeight_ * 4;
    resources::Buffer stagingBuffer(context_, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.upload(pixels.data(), imageSize);

    // 2. Transition to Transfer Det
    transitionImageLayout(image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // 3. Copy
    copyBufferToImage(stagingBuffer.handle(), image_, textureWidth_, textureHeight_);
    
    // 4. Transition back
    transitionImageLayout(image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // Store world dims for render
    int w = map.getWidth();
    int h = map.getHeight();
    worldWidth_ = w * config.resolution;
    worldHeight_ = h * config.resolution;
    
    // v3.8.0: Identify Symbols (Peaks)
    symbols_.clear();
    // Simple Local Maxima (Kernel 3x3 or larger step)
    // Simple Local Maxima (Kernel 3x3 or larger step)
    int step = 15; // v3.8.1: Finer step for Hills
    float peakThresh = std::max(config.waterLevel + 2.0f, config.maxHeight * 0.35f); // v3.8.1: Lower threshold (35%) to catch Hills
    
    for (int z = step; z < h - step; z += step) {
        for (int x = step; x < w - step; x += step) {
            float val = map.getHeight(x, z);
            if (val > peakThresh) {
                // Check neighbors
                bool isPeak = true;
                if (map.getHeight(x-step, z) >= val) isPeak = false;
                if (map.getHeight(x+step, z) >= val) isPeak = false;
                if (map.getHeight(x, z-step) >= val) isPeak = false;
                if (map.getHeight(x, z+step) >= val) isPeak = false;
                
                if (isPeak) {
                    float globalU = static_cast<float>(x) / w;
                    float globalV = static_cast<float>(z) / h; // v3.8.0 Fix: V aligned with Z
                    
                    // Actually, texture gen uses: v = static_cast<float>(y) / h;
                    // pixel y=0 is Top of texture. 
                    // map z=0 is usually Top or Bottom depending on coord sys.
                    // In texture gen: int mapZ = (1.0 - v) * mapH. So v=0 -> mapZ=mapH. v=1 -> mapZ=0.
                    // So Top of Texture (v=0) is Z=Max. Bottom (v=1) is Z=0.
                    // Wait, let's check generate loop: 
                    // for (uint32_t y = 0; y < textureHeight_; ++y) ... mapZ = (1.0f - v) * mapH;
                    // So Visual V matches 1.0 - Z_norm.
                    
                    symbols_.push_back({ globalU, globalV, 0 });
                }
            }
        }
    }
}

void Minimap::render(graphics::Camera& camera) {
    if (!textureID_) return;

    if (ImGui::Begin("Navigation (Minimap)", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Zoom/Pan Logic
        if (ImGui::IsWindowHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0) {
                zoomLevel_ -= wheel * 0.1f;
                zoomLevel_ = std::max(0.1f, std::min(1.0f, zoomLevel_));
            }
        }

        // Draw Image
        float visiblePortion = zoomLevel_;
        float halfVP = visiblePortion * 0.5f;

        // Clamp Center relative to visible portion
        centerX_ = std::max(halfVP, std::min(1.0f - halfVP, centerX_));
        centerY_ = std::max(halfVP, std::min(1.0f - halfVP, centerY_));

        ImVec2 uv0(centerX_ - halfVP, centerY_ - halfVP);
        ImVec2 uv1(centerX_ + halfVP, centerY_ + halfVP);

        // Display Size
        ImVec2 size(256, 256);
        ImVec2 pMin = ImGui::GetCursorScreenPos();
        
        ImGui::Image((ImTextureID)textureID_, size, uv0, uv1);

        // Interact: Click to Move Camera
        if (ImGui::IsItemHovered()) {
             if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                 ImVec2 mousePos = ImGui::GetMousePos();
                 float relX = (mousePos.x - pMin.x) / size.x;
                 float relY = (mousePos.y - pMin.y) / size.y;
                 
                 // Map back to Global UV
                 float globalU = uv0.x + relX * (uv1.x - uv0.x);
                 float globalV = uv0.y + relY * (uv1.y - uv0.y);

                 // Global UV to World Pos
                 float newX = globalU * worldWidth_;
                 float newZ = globalV * worldHeight_; // v3.8.0 Fix: V aligned with Z

                 // Update Camera
                 auto currentPos = camera.getPosition();
                 camera.teleportTo({newX, currentPos.y, newZ}); // v3.8.0 Fix
                 
                 // Also center map on new pos?
                 centerX_ = globalU;
                 centerY_ = globalV;
             }
             
             // Pan with Middle Mouse
             if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                 ImVec2 delta = ImGui::GetIO().MouseDelta;
                 float panScaleX = delta.x / size.x * visiblePortion; 
                 float panScaleY = delta.y / size.y * visiblePortion;
                 centerX_ -= panScaleX;
                 centerY_ -= panScaleY;
             }
        }

        // Overlay Player Position
        auto pos = camera.getPosition();
        float normX = pos.x / worldWidth_;
        float normZ = pos.z / worldHeight_;
        float normV = normZ; // v3.8.0 Fix: V aligned with Z. No inversion.

        // Normalize relative to current view
        float viewU = (normX - uv0.x) / (uv1.x - uv0.x);
        float viewV = (normV - uv0.y) / (uv1.y - uv0.y);

        if (viewU >= 0 && viewU <= 1 && viewV >= 0 && viewV <= 1) {
            float screenX = pMin.x + viewU * size.x;
            float screenY = pMin.y + viewV * size.y;
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Render Symbols (Allegories)
            // v3.8.1 Disabled: User found it cluttered or broken for Hills
            /*
            for (const auto& sym : symbols_) {
                float sU = (sym.u - uv0.x) / (uv1.x - uv0.x);
                float sV = (sym.v - uv0.y) / (uv1.y - uv0.y);
                
                if (sU >= 0 && sU <= 1 && sV >= 0 && sV <= 1) {
                    float sx = pMin.x + sU * size.x;
                    float sy = pMin.y + sV * size.y;
                    
                    if (sym.type == 0) { // Peak (Triangle)
                        drawList->AddTriangleFilled(
                            ImVec2(sx, sy - 4),
                            ImVec2(sx - 3, sy + 2),
                            ImVec2(sx + 3, sy + 2),
                            IM_COL32(255, 255, 255, 200)
                        );
                    }
                }
            }
            */

            // Player Icon
            drawList->AddCircleFilled(ImVec2(screenX, screenY), 4.0f, IM_COL32(255, 50, 50, 255));
            
            // Frustum Direction (Yaw)
            float yaw = camera.getYaw(); 
            float rad = (yaw - 90.0f) * 3.14159f / 180.0f; // Manual radians
            
            float dirX = std::cos(rad) * 10.0f;
            float dirY = std::sin(rad) * 10.0f; 
            
            // Draw Direction line
            drawList->AddLine(ImVec2(screenX, screenY), ImVec2(screenX + dirX, screenY + dirY), IM_COL32(255, 0, 0, 255), 2.0f);
        }
    }
    ImGui::End();
}

// Helpers
// Removed duplicate definition
// Implementation at end of file
// Redefining helpers to be self-contained in operations

void Minimap::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    core::CommandPool pool(context_, context_.queueFamilyIndex());
    VkCommandBuffer commandBuffer = pool.allocate(1)[0];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        // Fallback
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context_.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphicsQueue());
}

void Minimap::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    core::CommandPool pool(context_, context_.queueFamilyIndex());
    VkCommandBuffer commandBuffer = pool.allocate(1)[0];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context_.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphicsQueue());
}

uint32_t Minimap::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

VkCommandBuffer Minimap::beginSingleTimeCommands() { return VK_NULL_HANDLE; } 
void Minimap::endSingleTimeCommands(VkCommandBuffer) {}

} // namespace ui
