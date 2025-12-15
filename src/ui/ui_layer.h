#pragma once

#include "../graphics/camera.h"
#include "../graphics/camera.h"
#include "../graphics/voxel_terrain.h"
#include "../terrain/terrain_map.h" // v3.5.0
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
    terrain::TerrainMap* finiteMap; // v3.5.0
    bool& showSlopeAnalysis; // v3.5.2
    bool& showDrainage;      // v3.6.1
    float& drainageIntensity;

    bool& showErosion; // v3.6.2
    bool& showWatershedVis; // v3.6.3
    bool& showBasinOutlines; // v3.6.4
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
    using LoadBookmarkCallback = std::function<void(size_t)>;
    using DeleteBookmarkCallback = std::function<void(size_t)>;
    using RegenerateFiniteWorldCallback = std::function<void(int size, float scale, float amplitude, float resolution)>; // v3.6.5
    using RequestMeshUpdateCallback = std::function<void()>;
    LoadBookmarkCallback loadBookmark;
    DeleteBookmarkCallback deleteBookmark;
    std::function<void(int)> requestTerrainReset;
    std::function<void()> savePreferences; // v3.4.0
    std::function<void()> loadPreferences; // v3.4.0
    RegenerateFiniteWorldCallback regenerateFiniteWorld; // v3.5.1
    RequestMeshUpdateCallback updateMesh; // v3.6.3
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
    void drawFiniteTools(UiFrameContext& ctx); // v3.5.0

    Theme currentTheme_ = Theme::Dark;
    bool showStatsOverlay_ = true;
    bool showBookmarks_ = false;
    Callbacks callbacks_;
};

} // namespace ui
