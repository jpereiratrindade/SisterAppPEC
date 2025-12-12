#include "renderer.h"
#include "shader_loader.h"
#include "vk_utils.h"
#include "graphics/mesh.h"
#include "graphics/material.h"

#include <array>
#include <cstring>
#include <string>

namespace graphics {

void Renderer::bindPipeline(VkCommandBuffer cmd, const Material* material, VkExtent2D extent) const {
    if (!material) return;
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    material->bind(cmd);
}

void Renderer::record(VkCommandBuffer cmd, const Mesh* mesh, const Material* material, VkExtent2D extent, const float* mvp4x4, const RenderOptions& options) const {
    if (!material) return;

    bindPipeline(cmd, material, extent);

    if (mvp4x4) {
        RendererPushConstants pc{};

        std::memcpy(pc.mvp, mvp4x4, sizeof(float) * 16);
        pc.pointSize = options.pointSize;
        pc.useLighting = options.useLighting ? 1.0f : 0.0f;
        pc.useFixedColor = options.useFixedColor ? 1.0f : 0.0f;
        pc.opacity = options.opacity;
        pc.fixedColor[0] = options.fixedColor[0];
        pc.fixedColor[1] = options.fixedColor[1];
        pc.fixedColor[2] = options.fixedColor[2];
        pc.padding = 0.0f;
        pc.cameraPos[0] = options.cameraPos[0];
        pc.cameraPos[1] = options.cameraPos[1];
        pc.cameraPos[2] = options.cameraPos[2];

        vkCmdPushConstants(cmd, material->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    }

    if (mesh) {
        mesh->draw(cmd);
    }
}

} // namespace graphics
