#pragma once

#include "../graphics/camera.h"
#include "../graphics/voxel_terrain.h"
#include "../graphics/animation.h"
#include "../math/math_types.h"
#include "bookmark.h"

#include <functional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace ui {

enum class Theme { Dark, Light };

struct UiFrameContext {
    bool& running;
    bool& needsRecreate;
    bool& vsyncEnabled;
    bool& limitIdleFps;
    bool& fpsCapEnabled;
    float& fpsCapTarget;
    bool& animationEnabled;
    bool& showVegetation;
    graphics::Camera& camera;
    graphics::VoxelTerrain* terrain;
    int visibleChunks = 0;
    int totalChunks = 0;
    int pendingTasks = 0;
    int pendingVeg = 0;
    graphics::Animator& axesAnimator;
    std::vector<Bookmark>& bookmarks;
    std::string& lastSurfaceInfo;
    bool& lastSurfaceValid;
};

struct Callbacks {
    std::function<void(const std::string&)> saveBookmark;
    std::function<void(size_t)> loadBookmark;
    std::function<void(size_t)> deleteBookmark;
    std::function<void(int)> requestTerrainReset;
    std::function<void()> savePreferences; // v3.4.0
    std::function<void()> loadPreferences; // v3.4.0
};

class UiLayer {
public:
    explicit UiLayer(Callbacks callbacks);

    void render(UiFrameContext& ctx, VkCommandBuffer cmd);
    void applyTheme(Theme theme);

private:
    void drawStats(UiFrameContext& ctx);
    void drawMenuBar(UiFrameContext& ctx);
    void drawCamera(UiFrameContext& ctx);
    void drawAnimation(UiFrameContext& ctx);
    void drawBookmarks(UiFrameContext& ctx);
    void drawResetCamera(UiFrameContext& ctx);

    Theme currentTheme_ = Theme::Dark;
    bool showStatsOverlay_ = true;
    bool showBookmarks_ = false;
    Callbacks callbacks_;
};

} // namespace ui
