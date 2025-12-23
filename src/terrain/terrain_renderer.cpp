#include "terrain_renderer.h"
#include "../ml/ml_service.h"
#include "soil_palette.h"
#include "../graphics/geometry_utils.h"
#include <iostream>
#include <cstring>
#include <algorithm> // v3.9.0 for std::clamp
#include <cmath>

namespace shape {

TerrainRenderer::TerrainRenderer(const core::GraphicsContext& ctx, VkRenderPass renderPass, VkCommandPool commandPool) : ctx_(ctx), commandPool_(commandPool) {
    // Reuse existing basic shader for v1 (Vertex Color)
    auto vs = std::make_shared<graphics::Shader>(ctx_, "shaders/basic.vert.spv");
    auto fs = std::make_shared<graphics::Shader>(ctx_, "shaders/terrain.frag.spv");
    
    // v3.9.0: Create Vegetation Layout FIRST so Material knows it
    createVegetationResources(1, 1); // Dummy size, will resize in updateVegetation or assumes fixed

    // Create Material with VALID RenderPass!
    material_ = std::make_unique<graphics::Material>(ctx_, renderPass, VkExtent2D{1280,720}, vs, fs, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, true, true, vegDescLayout_); 
}
    // Material initialized.

// 1. Refactored buildMesh to use helper
void TerrainRenderer::buildMesh(const terrain::TerrainMap& map, float gridScale, const ml::MLService* mlService, int soilMode, bool useMLColor) {
    MeshData data = generateMeshData(map, gridScale, mlService, soilMode, useMLColor);
    uploadMesh(data);
}

void TerrainRenderer::uploadMesh(const MeshData& data) {
    // This MUST run on Main Thread (GPU Access)
    mesh_ = std::make_unique<graphics::Mesh>(ctx_, data.vertices, data.indices);
}

TerrainRenderer::MeshData TerrainRenderer::generateMeshData(const terrain::TerrainMap& map, float gridScale, const ml::MLService* mlService, int soilMode, bool useMLColor) {
    int w = map.getWidth();
    int h = map.getHeight();
    
    MeshData data;
    data.vertices.reserve(w * h);
    data.indices.reserve((w - 1) * (h - 1) * 6);

    std::vector<graphics::Vertex>& vertices = data.vertices;
    std::vector<uint32_t>& indices = data.indices;

    // 1. Generate Vertices
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            float height = map.getHeight(x, z);
            
            // Calculate Smooth Normal
            float hL = map.getHeight(x > 0 ? x-1 : x, z);
            float hR = map.getHeight(x < w-1 ? x+1 : x, z);
            float hD = map.getHeight(x, z > 0 ? z-1 : z);
            float hU = map.getHeight(x, z < h-1 ? z+1 : z);
            
            // Central differences
            float dx = (hR - hL); // run=2
            float dz = (hU - hD);
            // Normal = cross( (2, dx, 0), (0, dz, 2) ) -> normalized
            // Roughly (-dx, 2, -dz)
            
            float nx = -dx;
            float ny = 2.0f * gridScale; // Correctly scale run by gridScale
            float nz = -dz;
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            
            graphics::Vertex v{};
            v.pos[0] = static_cast<float>(x) * gridScale;
            v.pos[1] = height;
            v.pos[2] = static_cast<float>(z) * gridScale;
            
            v.normal[0] = nx / len;
            v.normal[1] = ny / len;
            v.normal[2] = nz / len;

            // Visualization Colors
            // Slope-based coloring (Default / Base)
            float slope = 1.0f - v.normal[1]; // 0 = flat, 1 = vertical
            
            if (slope < 0.15f) { // Flat (Soil/Dirt)
                v.color[0] = 0.55f; v.color[1] = 0.47f; v.color[2] = 0.36f; // Light Brown
            } else if (slope < 0.4f) { // Hill (Darker Soil/Rock mix)
                v.color[0] = 0.45f; v.color[1] = 0.38f; v.color[2] = 0.31f; // Darker Brown
            } else { // Cliff (Rock)
                v.color[0] = 0.4f; v.color[1] = 0.4f; v.color[2] = 0.45f; // Blue-Grey Rock
            }
            
            // v4.5.1: SCORPAN Continuous Visualization (Mode 1)
            // Overrides slope color with S-Vector derived color
            if (soilMode == 1 && map.getLandscapeSoil()) {
                int idx = z * w + x;
                auto* soil = map.getLandscapeSoil();
                float depth = soil->depth[idx];
                float om = soil->organic_matter[idx]; // 0..1
                float clay = soil->clay_fraction[idx]; // 0..1
                float sand = soil->sand_fraction[idx]; // 0..1
                
                // v4.5.8: Force Semantic Coloring (SiBCS Map)
                // The user explicitly requested to see the Classification, not the blend.
                auto storedType = static_cast<terrain::SoilType>(soil->soil_type[idx]);
                
                // Get Color from Palette (SiBCS definitions)
                float rgb[3];
                // using terrain::SoilPalette via absolute include or verify if included
                // We need to verify include. If not present, we can't use SoilPalette static.
                // Assuming we add the include or just hardcode the mapping here for safety/speed 
                // to avoid include hell if TerrainRenderer doesn't know SoilPalette.
                // But Renderer usually knows simple types.
                // Let's assume we can include it. If not, I will add the include in a separate step or just copy logic.
                // Copying logic is safer to avoid rebuilds of headers.
                
                // v4.5.1: Fix - Use Centralized Palette with Suborder Support
                auto storedSub = static_cast<landscape::SiBCSSubOrder>(soil->suborder[idx]);
                terrain::SoilPalette::getFloatColor(storedType, storedSub, rgb);
                
                v.color[0] = rgb[0];
                v.color[1] = rgb[1];
                v.color[2] = rgb[2];
            }
            
            // v4.0.0 ML Override (Optional - takes precedence if active)
            if (mlService && useMLColor && map.getLandscapeSoil()) {
                int idx = z * w + x;
                auto* soil = map.getLandscapeSoil();
                float d = soil->depth[idx];
                float om = soil->organic_matter[idx];
                float inf = soil->infiltration[idx] / 100.0f;
                float comp = soil->compaction[idx];
                
                // Predict
                Eigen::Vector3f mlColor = mlService->predictSoilColor(d, om, inf, comp);
                v.color[0] = mlColor.x();
                v.color[1] = mlColor.y();
                v.color[2] = mlColor.z();
            }
            
            // v3.6.1 Flux (Drainage) Visualization
            // Store flux in UV.x for shader-based visualization toggling.
            float flux = map.fluxMap()[z * w + x];
            v.uv[0] = flux;
            
            // v3.6.2 Erosion (Sediment) Visualization
            // Store sediment in UV.y
            float sed = map.sedimentMap()[z * w + x];
            v.uv[1] = sed;

            // v3.6.3 Watershed Visualization
            // Store Basin ID in auxiliary
            v.auxiliary = static_cast<float>(map.watershedMap()[z * w + x]);
            
            // v3.7.3 Semantic Soil ID
            v.soilId = static_cast<float>(map.soilMap()[z * w + x]);
            
            vertices.push_back(v);
        }
    }

    // Indices (same as before)
    for (int z = 0; z < h - 1; ++z) {
        for (int x = 0; x < w - 1; ++x) {
            int topLeft = z * w + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * w + x;
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    return data;
}

void TerrainRenderer::render(VkCommandBuffer cmd, const std::array<float, 16>& mvp, VkExtent2D viewport, 
                             bool showSlopeVis, bool showDrainageVis, float drainageIntensity, 
                             bool showWatershedVis, bool showBasinOutlines, bool showSoilVis,
                             bool soilHidroAllowed, bool soilBTextAllowed, bool soilArgilaAllowed, 
                             bool soilBemDesAllowed, bool soilRasoAllowed, bool soilRochaAllowed,
                             float sunAzimuth, float sunElevation, float fogDensity, float lightIntensity, float uvScale, int vegetationMode) {
    if (!mesh_ || !material_) return;
    
    // Bind Pipeline and Descriptor Sets
    material_->bind(cmd);
    
    // Bind Vegetation Descriptor Set (Set 0) if available
    if (vegDescSet_ != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material_->layout(), 0, 1, &vegDescSet_, 0, nullptr);
    }

    // Define PC struct matching shader
    // Unified PushConstants Layout (Aligned)
    struct PushConstantsPacked {
        float mvp[16];
        float sunDir[4];
        float fixedColor[4];
        float params[4]; // x=opacity, y=drainageIntensity, z=fogDensity, w=pointSize
        uint32_t flags;  // Bitmask: 1=Lit, 2=FixCol, 4=Slope, 8=Drain, 16=Eros, 32=Water, 64=Soil, 128=Basin
        float pad[3];
    } pc;
    static_assert(sizeof(pc) == 128, "PC size mismatch");

    std::memcpy(pc.mvp, mvp.data(), sizeof(float) * 16);
    
    // SunDir (Normalize here)
    float toRad = 3.14159265f / 180.0f;
    float radAz = sunAzimuth * toRad;
    float radEl = sunElevation * toRad;
    pc.sunDir[0] = std::cos(radEl) * std::sin(radAz);
    pc.sunDir[1] = std::sin(radEl);
    pc.sunDir[2] = std::cos(radEl) * std::cos(radAz);
    pc.sunDir[3] = uvScale; // v3.9.0 Vegetation UV Scale (1.0 / worldSize)

    pc.fixedColor[0] = 1.0f; pc.fixedColor[1] = 1.0f; pc.fixedColor[2] = 1.0f; pc.fixedColor[3] = 1.0f;
    
    pc.params[0] = 1.0f; // Opacity
    pc.params[1] = drainageIntensity;
    pc.params[2] = fogDensity;
    pc.params[3] = lightIntensity; // v3.8.1 Light Intensity

    // Pack Flags
    int mask = 0;
    if (showSlopeVis || showSoilVis) { /* Lighting logic? User says FORCE it. */ mask |= 1; } // Use Lighting Always?
    // Wait, step 1258 says "pc.useLighting = 1.0f" ALWAYS.
    mask |= 1; // lighting default ON
    
    if (false) mask |= 2; // FixedColor unused currently or 0
    if (showSlopeVis) mask |= 4;
    if (showDrainageVis) mask |= 8;
    // Erosion? Internal/Deprecated, let's keep slot: mask |= 16;
    if (showWatershedVis) mask |= 32;
    if (showSoilVis) mask |= 64;
    if (showBasinOutlines) mask |= 128;
    
    // Soil Whitelist Bits (8-13)
    if (soilHidroAllowed)  mask |= 256;
    if (soilBTextAllowed)  mask |= 512;
    if (soilArgilaAllowed) mask |= 1024;
    if (soilBemDesAllowed) mask |= 2048;
    if (soilRasoAllowed)   mask |= 4096;
    if (soilRochaAllowed)  mask |= 8192;
    
    // v3.9.0 Vegetation Mode (Bits 16-19)
    if (vegetationMode > 0) {
        mask |= ((vegetationMode & 0xF) << 16);
    }
    
    pc.flags = static_cast<uint32_t>(mask);

    vkCmdPushConstants(cmd, material_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Draw using Mesh API
    mesh_->draw(cmd);
}

// v3.9.0 Vegetation Implementation
void TerrainRenderer::createVegetationResources(int width, int height) {
    auto device = ctx_.device();
    vegWidth_ = width;
    vegHeight_ = height;
    

    // 1. Create Descriptor Layout
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &vegDescLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // 2. Create Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &vegDescPool_) != VK_SUCCESS) {
         throw std::runtime_error("failed to create descriptor pool!");
    }

    // 3. Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vegDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vegDescLayout_;

    if (vkAllocateDescriptorSets(device, &allocInfo, &vegDescSet_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // 4. Create Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    if (vkCreateImage(device, &imageInfo, nullptr, &vegImage_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vegetation image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, vegImage_, &memRequirements);

    VkMemoryAllocateInfo allocImgInfo{};
    allocImgInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocImgInfo.allocationSize = memRequirements.size;
    allocImgInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocImgInfo, nullptr, &vegMemory_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vegetation image memory!");
    }

    vkBindImageMemory(device, vegImage_, vegMemory_, 0);

    // 5. Create View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vegImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &vegView_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    // 6. Create Sampler
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
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &vegSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    // 7. Update Descriptor Set
    VkDescriptorImageInfo descImageInfo{};
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descImageInfo.imageView = vegView_;
    descImageInfo.sampler = vegSampler_;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = vegDescSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &descImageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void TerrainRenderer::updateVegetation(const vegetation::VegetationGrid& grid) {

    if (!grid.isValid()) return;

    // 1. Initialization / Resize Check
    if (vegImage_ == VK_NULL_HANDLE || vegWidth_ != grid.width || vegHeight_ != grid.height) {
        if (vegImage_ != VK_NULL_HANDLE) {
            destroyVegetationResources();
        }
        createVegetationResources(grid.width, grid.height);
        
        // Ensure Fence and Command Buffer exist
        if (uploadFence_ == VK_NULL_HANDLE) {
            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = 0; // Starts unsignaled
            vkCreateFence(ctx_.device(), &fenceInfo, nullptr, &uploadFence_);
            
            // Allocate Command Buffer once
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = commandPool_;
            allocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(ctx_.device(), &allocInfo, &uploadCmd_);
        }
    }

    VkDeviceSize imageSize = grid.width * grid.height * 4;

    // 2. Wait for previous upload to complete (Back-pressure)
    if (hasPendingUpload_) {
        vkWaitForFences(ctx_.device(), 1, &uploadFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(ctx_.device(), 1, &uploadFence_);
    }

    // 3. Staging Buffer Management
    if (imageSize > stagingSize_ || stagingBuffer_ == VK_NULL_HANDLE) {
        // Free old
        if (stagingBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_.device(), stagingBuffer_, nullptr);
            vkFreeMemory(ctx_.device(), stagingMemory_, nullptr);
        }
        // Allocate new (Host Visible)
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     stagingBuffer_, stagingMemory_);
        stagingSize_ = imageSize;
    }

    // 4. Map and Fill (Parallel)
    void* data;
    vkMapMemory(ctx_.device(), stagingMemory_, 0, imageSize, 0, &data);
    
    uint8_t* pixels = static_cast<uint8_t*>(data);
    size_t count = grid.getSize();

    // PARALLEL CONVERSION (Critical for 4K)
    #pragma omp parallel for
    for (size_t i = 0; i < count; ++i) {
        pixels[i * 4 + 0] = static_cast<uint8_t>(std::clamp(grid.ei_coverage[i], 0.0f, 1.0f) * 255.0f);
        pixels[i * 4 + 1] = static_cast<uint8_t>(std::clamp(grid.es_coverage[i], 0.0f, 1.0f) * 255.0f);
        pixels[i * 4 + 2] = static_cast<uint8_t>(std::clamp(grid.ei_vigor[i], 0.0f, 1.0f) * 255.0f);
        pixels[i * 4 + 3] = static_cast<uint8_t>(std::clamp(grid.es_vigor[i], 0.0f, 1.0f) * 255.0f);
    }
    
    vkUnmapMemory(ctx_.device(), stagingMemory_);

    // 5. Submit Upload Command
    vkResetCommandBuffer(uploadCmd_, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(uploadCmd_, &beginInfo);
    
    // Transition Undefined/Read -> Transfer Dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Or SHADER_READ_ONLY if preserving? We overwrite all, so UNDEFINED is fine/faster.
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vegImage_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(uploadCmd_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { (uint32_t)grid.width, (uint32_t)grid.height, 1 };

    vkCmdCopyBufferToImage(uploadCmd_, stagingBuffer_, vegImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition Transfer Dst -> Shader Read Only
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(uploadCmd_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(uploadCmd_);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &uploadCmd_;

    // Submit with Fence
    vkQueueSubmit(ctx_.graphicsQueue(), 1, &submitInfo, uploadFence_);
    hasPendingUpload_ = true;
}

void TerrainRenderer::destroyVegetationResources() {
    VkDevice device = ctx_.device();
    if (vegSampler_) vkDestroySampler(device, vegSampler_, nullptr);
    if (vegView_) vkDestroyImageView(device, vegView_, nullptr);
    if (vegImage_) vkDestroyImage(device, vegImage_, nullptr);
    if (vegMemory_) vkFreeMemory(device, vegMemory_, nullptr);
    if (vegDescPool_) vkDestroyDescriptorPool(device, vegDescPool_, nullptr);
    if (vegDescLayout_) vkDestroyDescriptorSetLayout(device, vegDescLayout_, nullptr);
    
    // Clean up Async Resources
    if (stagingBuffer_) vkDestroyBuffer(device, stagingBuffer_, nullptr);
    if (stagingMemory_) vkFreeMemory(device, stagingMemory_, nullptr);
    if (uploadFence_) vkDestroyFence(device, uploadFence_, nullptr);
    // Command Buffer is freed with pool usually, but to be clean:
    if (uploadCmd_) vkFreeCommandBuffers(device, commandPool_, 1, &uploadCmd_);
    
    vegSampler_ = VK_NULL_HANDLE;
    vegView_ = VK_NULL_HANDLE;
    vegImage_ = VK_NULL_HANDLE;
    vegMemory_ = VK_NULL_HANDLE;
    vegDescPool_ = VK_NULL_HANDLE;
    vegDescLayout_ = VK_NULL_HANDLE;
    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    uploadFence_ = VK_NULL_HANDLE;
    uploadCmd_ = VK_NULL_HANDLE;
    stagingSize_ = 0;
    hasPendingUpload_ = false;
}

uint32_t TerrainRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(ctx_.physicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void TerrainRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx_.device(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(ctx_.device(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_.device(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) throw std::runtime_error("failed to allocate buffer memory!");
    vkBindBufferMemory(ctx_.device(), buffer, bufferMemory, 0);
}

} // namespace shape
