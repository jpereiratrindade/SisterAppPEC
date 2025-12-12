#pragma once

#include "renderer.h"
#include "graphics/voxel_terrain.h"
#include "graphics/material.h"
#include "graphics/camera.h"
#include "math/frustum.h"
#include "math/math_types.h"
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

struct VoxelSceneStats {
    int visibleChunks = 0;
    int totalChunks = 0;
    int pendingTasks = 0;
    int pendingVeg = 0;
};

/**
 * @brief Encapsulates voxel-specific update/render logic away from Application.
 */
class VoxelScene {
public:
    VoxelScene(graphics::VoxelTerrain* terrain, graphics::Material* voxelMat, graphics::Material* waterMat);

    void setTerrain(graphics::VoxelTerrain* terrain);
    void setMaterials(graphics::Material* voxelMat, graphics::Material* waterMat);
    void setRenderer(graphics::Renderer* renderer);

    void requestReset(int warmupRadius = 1);
    void applyPendingReset(uint32_t nowMs, uint32_t cooldownMs);

    void update(double dt, graphics::Camera& camera, int frameIndex);
    void render(VkCommandBuffer cmd, const float* view, const float* proj, const graphics::Camera& cam, VoxelSceneStats& outStats, VkExtent2D extent) const;

    bool probeSurface(const math::Ray& ray, float maxDistance, std::string& outInfo, bool& outValid) const;

    graphics::VoxelTerrain* terrain() const { return terrain_; }

private:
    static void mulMat4(const float a[16], const float b[16], float out[16]);

    graphics::VoxelTerrain* terrain_ = nullptr;      // Not owned
    graphics::Material* voxelMaterial_ = nullptr;    // Not owned
    graphics::Material* waterMaterial_ = nullptr;    // Not owned
    graphics::Renderer* renderer_ = nullptr;         // Not owned

    bool pendingTerrainReset_ = false;
    int pendingTerrainWarmupRadius_ = 1;
    uint32_t lastTerrainResetMs_ = 0;
};
