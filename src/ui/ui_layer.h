#pragma once

#include "../graphics/camera.h"
#include "../graphics/camera.h"
// VoxelTerrain removed
#include "../terrain/terrain_map.h" // v3.5.0
#include "../vegetation/vegetation_types.h" // v3.9.0
#include "../graphics/animation.h"
#include "../math/math_types.h"
#include "bookmark.h"

#include <functional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "minimap.h" // v3.8.0

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
    // showVegetation removed
    graphics::Camera& camera;
    // VoxelTerrain pointer removed
    terrain::TerrainMap* finiteMap; // v3.5.0
    bool& showSlopeAnalysis; // v3.5.2
    bool& showDrainage;      // v3.6.1
    float& drainageIntensity;

    bool& showErosion; // v3.6.2
    bool& showWatershedVis; // v3.6.3
    bool& showBasinOutlines; // v3.6.4
    bool& showSoilVis; // v3.7.0
    // v3.7.5 Soil Whitelist
    bool& soilHidroAllowed;
    bool& soilBTextAllowed;
    bool& soilArgilaAllowed;
    bool& soilBemDesAllowed;
    bool& soilRasoAllowed;
    bool& soilRochaAllowed;

    // v3.7.3 Visual Controls
    float& sunAzimuth;
    float& sunElevation;
    float& fogDensity;
    // Voxel Stats Removed
    graphics::Animator& axesAnimator;
    std::vector<Bookmark>& bookmarks;
    std::string& lastSurfaceInfo;
    bool& lastSurfaceValid;
    float* lastSurfaceColor; // 3 floats
    
    // v3.7.8: Seeding & Resolution
    int& seed;
    float& worldResolution;
    // v3.8.1 Light
    float& lightIntensity;
    // v3.8.3 Async Status
    bool isRegenerating;
    
    // v3.9.0 Vegetation
    int& vegetationMode;
    vegetation::DisturbanceRegime& disturbanceParams;
    
    // v4.0.0 Hydro
    float& rainIntensity;
};

struct Callbacks {
    std::function<void(const std::string&)> saveBookmark;
    using LoadBookmarkCallback = std::function<void(size_t)>;
    using DeleteBookmarkCallback = std::function<void(size_t)>;
    using RegenerateFiniteWorldCallback = std::function<void(const terrain::TerrainConfig& config)>; // v3.8.3 Struct-based
    using RequestMeshUpdateCallback = std::function<void()>;
    LoadBookmarkCallback loadBookmark;
    DeleteBookmarkCallback deleteBookmark;
    std::function<void(int)> requestTerrainReset;
    std::function<void()> savePreferences; // v3.4.0
    std::function<void()> loadPreferences; // v3.4.0
    std::function<void()> resetVegetation; // v3.9.1: Refresh button
    std::function<void()> triggerFireEvent; // v3.9.2: Manual Fire
    RegenerateFiniteWorldCallback regenerateFiniteWorld; // v3.5.1
    RequestMeshUpdateCallback updateMesh; // v3.6.3
};

// ... (Moved include to top)

class UiLayer {
public:
    explicit UiLayer(const core::GraphicsContext& context, Callbacks callbacks);

    void render(UiFrameContext& ctx, VkCommandBuffer cmd);
    void applyTheme(Theme theme);
    
    // v3.8.0: Minimap Trigger
    void onTerrainUpdated(const terrain::TerrainMap& map, const terrain::TerrainConfig& config);

    // v3.8.0: Toggle
    bool& showMinimap() { return showMinimap_; }

private:
    void drawStats(UiFrameContext& ctx);
    void drawMenuBar(UiFrameContext& ctx);
    void drawCamera(UiFrameContext& ctx);
    void drawAnimation(UiFrameContext& ctx);
    void drawBookmarks(UiFrameContext& ctx);
    void drawResetCamera(UiFrameContext& ctx);
    void drawFiniteTools(UiFrameContext& ctx); 
    void drawCrosshair(UiFrameContext& ctx); // v3.8.1 

    Theme currentTheme_ = Theme::Dark;
    bool showStatsOverlay_ = true;
    bool showBookmarks_ = false;
    // v3.8.0
    // v3.8.0
    bool showMinimap_ = true;
    // v3.8.1: Views Toggles
    bool showMapGenerator_ = true;
    bool showCamControls_ = true;
    bool showResetCamera_ = true;
    bool showDebugInfo_ = false; // Default off
    
    Callbacks callbacks_;
    std::unique_ptr<Minimap> minimap_;
};

} // namespace ui
