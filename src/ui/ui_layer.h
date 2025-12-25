#pragma once

#include "../graphics/camera.h"
// VoxelTerrain removed
#include "../terrain/terrain_map.h" // v3.5.0
#include "../terrain/landscape_metrics.h" // v4.3.5
#include "../vegetation/vegetation_types.h" // v3.9.0
#include "../graphics/animation.h"
#include "../math/math_types.h"
#include "../landscape/soil_services.h" // v4.5.1
#include "bookmark.h"

#include <functional>
#include <string>
#include <vector>
#include <map>
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
    
    // v4.0.0 ML
    bool& showMLSoil;
    size_t mlDatasetSize;
    size_t mlHydroDatasetSize; // v4.2.0
    size_t mlFireDatasetSize;  // v4.2.1
    size_t mlGrowthDatasetSize;// v4.2.1
    bool isTraining;
    
    // v4.2.1 Advanced ML Controls
    int& mlTrainingEpochs;
    float& mlLearningRate;
    int& mlSampleCount;

    // v4.5.1 SCORPAN Parameters
    landscape::Climate& soilClimate;
    landscape::OrganismPressure& soilOrganism;
    landscape::ParentMaterial& soilParentMaterial;
    int& soilClassificationMode; // 1=SCORPAN (sole supported mode)
    landscape::SiBCSUserConfig* sibcsConfig; // v4.6.0
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
    std::function<void()> recomputeSoil; // v4.4.2: Recalculate Soil System Only
    
    // SCORPAN-only visualization toggle (int kept for API compatibility)
    std::function<void(int)> switchSoilMode;
    std::function<void(int, float)> mlTrainModel;

    // v4.0.0 ML Hooks (Restored)
    std::function<void(int)> mlCollectData;

    // v4.2.0 Hydro ML
    std::function<void(int)> mlCollectHydroData;
    std::function<void(int, float)> mlTrainHydroModel;
    
    // v4.2.1 Phase 2 ML Hooks
    std::function<void(int)> mlCollectFireData;
    std::function<void(int, float)> mlTrainFireModel;
    std::function<void(int)> mlCollectGrowthData;
    std::function<void(int, float)> mlTrainGrowthModel;
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
    void drawToolbar(UiFrameContext& ctx);
    void drawInspector(UiFrameContext& ctx);
    
    // Inspector Content Providers
    void drawTerrainInspector(UiFrameContext& ctx);
    void drawHydrologyInspector(UiFrameContext& ctx);
    void drawSoilInspector(UiFrameContext& ctx);
    void drawMLInspector(UiFrameContext& ctx);   
    void drawVegetationInspector(UiFrameContext& ctx); 
    void drawCrosshair(UiFrameContext& ctx);

    Theme currentTheme_ = Theme::Dark;
    bool showStatsOverlay_ = true;
    bool showBookmarks_ = false;
    bool showMinimap_ = true;

    enum class ActiveTool { None, Terrain, Hydro, Soil, Vegetation, MLHub }; // Removed Geology
    ActiveTool activeTool_ = ActiveTool::Terrain; // Default to Terrain
    bool showCamControls_ = true;
    bool showResetCamera_ = true;
    bool showDebugInfo_ = false; // Default off
    
    Callbacks callbacks_;
    std::unique_ptr<Minimap> minimap_;

    // Generation State (Refactored v4.3.2)
    int genSelectedSize_ = 1024;
    float genScale_ = 0.0010f; // Matches terrain_map.h
    float genAmplitude_ = 80.0f;
    int genPreset_ = 1; 
    float genPersistence_ = 0.40f; // Matches terrain_map.h
    float genWaterLvl_ = 64.0f;
    int genSeedInput_ = 12345;
    bool genUseBlend_ = false;
    float genBlendLow_ = 1.0f;
    float genBlendMid_ = 0.5f;
    float genBlendHigh_ = 0.25f;
    float genBlendExp_ = 1.0f;
    float genResolution_ = 1.0f;

    // v4.3.5: Metrics Cache
    std::map<terrain::SoilType, terrain::ClassMetrics> lastMetrics_;
    double lastMetricsCalcTime_ = 0.0;
};

} // namespace ui
