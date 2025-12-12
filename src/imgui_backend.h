#pragma once

#include "core/graphics_context.h"
#include "init_sdl.h"

#include <cstddef>
#include <vulkan/vulkan.h>

bool initImGuiCore(SDLContext& sdl, core::GraphicsContext& ctx, VkDescriptorPool& descriptorPool);
bool initImGuiVulkan(core::GraphicsContext& ctx, VkRenderPass renderPass, uint32_t imageCount, VkDescriptorPool descriptorPool);
void shutdownImGuiVulkanIfNeeded();
void beginGuiFrame();
void endGuiFrame();
void drawDebugGui(float dtSeconds, std::size_t entityCount);
