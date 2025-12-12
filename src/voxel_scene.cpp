#include "voxel_scene.h"

#include "graphics/material.h"
#include "graphics/mesh.h"
#include "math/frustum.h"

#include <cstring>

VoxelScene::VoxelScene(graphics::VoxelTerrain* terrain, graphics::Material* voxelMat, graphics::Material* waterMat)
    : terrain_(terrain), voxelMaterial_(voxelMat), waterMaterial_(waterMat) {}

void VoxelScene::setTerrain(graphics::VoxelTerrain* terrain) {
    terrain_ = terrain;
}

void VoxelScene::setMaterials(graphics::Material* voxelMat, graphics::Material* waterMat) {
    voxelMaterial_ = voxelMat;
    waterMaterial_ = waterMat;
}

void VoxelScene::setRenderer(graphics::Renderer* renderer) {
    renderer_ = renderer;
}

void VoxelScene::requestReset(int warmupRadius) {
    pendingTerrainReset_ = true;
    pendingTerrainWarmupRadius_ = warmupRadius;
}

void VoxelScene::applyPendingReset(uint32_t nowMs, uint32_t cooldownMs) {
    if (!terrain_ || !pendingTerrainReset_) return;
    if (nowMs - lastTerrainResetMs_ < cooldownMs) return;
    terrain_->reset(pendingTerrainWarmupRadius_);
    pendingTerrainReset_ = false;
    lastTerrainResetMs_ = nowMs;
}

void VoxelScene::update(double dt, graphics::Camera& camera, int frameIndex) {
    if (!terrain_) return;

    auto camPos = camera.getPosition();
    
    // Calculate Frustum for culling priority
    float view[16];
    float proj[16];
    camera.getViewMatrix(view);
    camera.getProjectionMatrix(proj);
    float mvp[16];
    mulMat4(proj, view, mvp); // VoxelScene::mulMat4 is available
    math::Frustum frustum = math::extractFrustum(mvp);

    terrain_->update(camPos.x, camPos.z, frustum, frameIndex);

    if (camera.getCameraMode() == graphics::CameraMode::FreeFlight) {
        camera.applyGravity(static_cast<float>(dt));
        camera.checkTerrainCollision(*terrain_);
    }

    camera.update(static_cast<float>(dt));
}

void VoxelScene::mulMat4(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[c * 4 + r] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
            }
        }
    }
}

void VoxelScene::render(VkCommandBuffer cmd, const float* view, const float* proj, const graphics::Camera& cam, VoxelSceneStats& outStats, VkExtent2D extent) const {
    outStats = {};
    if (!terrain_ || !voxelMaterial_ || !renderer_) {
        return;
    }

    float mvp[16];
    mulMat4(proj, view, mvp);

    math::Frustum frustum = math::extractFrustum(mvp);
    auto visibleChunks = terrain_->getVisibleChunks(frustum);

    outStats.visibleChunks = static_cast<int>(visibleChunks.size());
    outStats.totalChunks = terrain_->chunkCount();
    outStats.pendingTasks = terrain_->pendingTaskCount();
    outStats.pendingVeg = terrain_->pendingVegetationCount();

    renderer_->bindPipeline(cmd, voxelMaterial_, extent);

    graphics::RendererPushConstants pc{};
    std::memcpy(pc.mvp, mvp, sizeof(float) * 16);
    pc.pointSize = 1.0f;
    pc.useLighting = 1.0f;
    pc.useFixedColor = 0.0f;
    pc.opacity = 1.0f;
    pc.fixedColor[0] = 1.0f;
    pc.fixedColor[1] = 1.0f;
    pc.fixedColor[2] = 1.0f;
    pc.padding = 0.0f;
    auto camPos = cam.getPosition();
    pc.cameraPos[0] = camPos.x;
    pc.cameraPos[1] = camPos.y;
    pc.cameraPos[2] = camPos.z;
    vkCmdPushConstants(cmd, voxelMaterial_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    for (auto* chunk : visibleChunks) {
        if (!chunk) continue;
        auto mesh = chunk->getMeshShared();
        if (mesh) {
            mesh->draw(cmd);
        }
    }

    if (waterMaterial_) {
        renderer_->bindPipeline(cmd, waterMaterial_, extent);
        graphics::RendererPushConstants pcWater = pc;
        pcWater.opacity = 0.55f;
        vkCmdPushConstants(cmd, waterMaterial_->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pcWater), &pcWater);
        for (auto* chunk : visibleChunks) {
            if (!chunk) continue;
            auto waterMesh = chunk->getWaterMesh();
            if (waterMesh) {
                waterMesh->draw(cmd);
            }
        }
    }
}

bool VoxelScene::probeSurface(const math::Ray& ray, float maxDistance, std::string& outInfo, bool& outValid) const {
    outValid = false;
    if (!terrain_) return false;

    graphics::SurfaceHit hit;
    if (terrain_->probeSurface(ray, maxDistance, hit) && hit.valid) {
        outValid = true;
        std::string cls = "Flat (0-3%)";
        if (hit.terrainClass == graphics::TerrainClass::Slope) cls = "Gentle Slope";
        else if (hit.terrainClass == graphics::TerrainClass::Mountain) cls = "Steep/Mountain";

        std::string vegCls = "None";
        if (hit.vegetation == graphics::VegetationClass::Sparse) vegCls = "Sparse";
        else if (hit.vegetation == graphics::VegetationClass::Rich) vegCls = "Rich";

        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), 
            "Surface: %s | Slope: %.1f%% | Veg: %s | Wet: %.2f @ (%d, %d, %d)",
            cls.c_str(), hit.slopePct, vegCls.c_str(), hit.moisture, hit.worldX, hit.worldY, hit.worldZ);
        
        outInfo = std::string(buffer);
        return true;
    }
    return false;
}
