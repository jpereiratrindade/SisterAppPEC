#include "ui_layer.h"
#include "../landscape/lithology_registry.h"

#include "../imgui_backend.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "../terrain/hydrology_report.h"
#include "../terrain/landscape_metrics.h"
#include "../terrain/pattern_validator.h" // v4.3.0 DDD
#include "../terrain/watershed.h"
#include "../terrain/soil_palette.h"
#include <fstream>

#include <array>
#include <iostream>
#include <utility>
#include <algorithm>

namespace ui {

UiLayer::UiLayer(const core::GraphicsContext& context, Callbacks callbacks)
    : callbacks_(std::move(callbacks)) {
    minimap_ = std::make_unique<Minimap>(context);
}

void UiLayer::onTerrainUpdated(const terrain::TerrainMap& map, const terrain::TerrainConfig& config) {
    if (minimap_) minimap_->update(map, config);
}

void UiLayer::render(UiFrameContext& ctx, VkCommandBuffer cmd) {
    beginGuiFrame();
    if (showDebugInfo_) drawDebugGui(0.0f, 0);

    drawStats(ctx);
    drawMenuBar(ctx);
    // drawAnimation(ctx); // Removed on User Request
    drawBookmarks(ctx);
    if (showResetCamera_) drawResetCamera(ctx);
    // Draw calls moved to end of function to ensure they are on top or ordered correctly
    // See Domain Windows section below
    // v3.8.1 Use showCamControls_ for Viewer Controls (drawCamera)
    if (showCamControls_) drawCamera(ctx); 
    else {
        // Even if hidden, shortcuts should work?
    }
    drawCrosshair(ctx); // v3.8.1
    
    // v3.8.0 Minimap
    if (showMinimap_ && minimap_) {
        minimap_->render(ctx.camera);
    }

    // v3.8.3 Loading Overlay
    if (ctx.isRegenerating) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 100));
        if (ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Generating Terrain...");
            ImGui::Separator();
            ImGui::TextWrapped("Please wait. Large maps (4096) can take several seconds to process 16 million cells.");
        }
        ImGui::End();
    }

    // Domain Windows (v4.1.0) -> Toolbar + Inspector (v4.3.0)
    drawToolbar(ctx);
    drawInspector(ctx);

    endGuiFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UiLayer::drawStats(UiFrameContext& ctx) {
    if (!showStatsOverlay_) return;

    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 200), ImGuiCond_FirstUseEver); // Allow moving
    ImGui::SetNextWindowBgAlpha(0.65f);
    // Allow Decoration (Title Bar) so it can be moved. Disallow Collapse for simplicity.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | 
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Probe & Stats", &showStatsOverlay_, flags)) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Mode: %s", ctx.camera.getCameraMode() == graphics::CameraMode::FreeFlight ? "Free Flight" : "Orbital");
        auto pos = ctx.camera.getPosition();
        ImGui::Text("Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        ImGui::Separator();
        ImGui::Text("Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        // Stats removed
        
        if (ctx.lastSurfaceValid) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Probe Results:");
            
            // Color Display
            float* c = ctx.lastSurfaceColor;
            ImGui::ColorButton("##probeColor", ImVec4(c[0], c[1], c[2], 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", ctx.lastSurfaceInfo.c_str());
        }
    }
    ImGui::End();
}

void UiLayer::drawMenuBar(UiFrameContext& ctx) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                ctx.running = false;
            }
            ImGui::EndMenu();
            // Removed extra ImGui::EndMenu() here
        }
        // v3.8.1: Views Menu
        if (ImGui::BeginMenu("Views")) {
             if (ImGui::MenuItem("Terrain & Environment", nullptr, activeTool_ == ActiveTool::Terrain)) activeTool_ = ActiveTool::Terrain;
             if (ImGui::MenuItem("Hydrology Tools", nullptr, activeTool_ == ActiveTool::Hydro)) activeTool_ = ActiveTool::Hydro;
             if (ImGui::MenuItem("Soil Inspector", nullptr, activeTool_ == ActiveTool::Soil)) activeTool_ = ActiveTool::Soil;
             if (ImGui::MenuItem("Vegetation Tools", nullptr, activeTool_ == ActiveTool::Vegetation)) activeTool_ = ActiveTool::Vegetation;
             if (ImGui::MenuItem("ML Service Hub", nullptr, activeTool_ == ActiveTool::MLHub)) activeTool_ = ActiveTool::MLHub;
             ImGui::Separator();
             ImGui::MenuItem("Minimap", nullptr, &showMinimap_);
             ImGui::MenuItem("Camera Controls", nullptr, &showCamControls_);
             ImGui::MenuItem("Probe & Stats", nullptr, &showStatsOverlay_);
             ImGui::MenuItem("Debug Info", nullptr, &showDebugInfo_);
             ImGui::Separator();
             ImGui::MenuItem("Reset Camera Button", nullptr, &showResetCamera_);
             if (ImGui::MenuItem("Reset Camera Now", "R")) {
                 ctx.camera.reset();
                 if (callbacks_.requestTerrainReset) callbacks_.requestTerrainReset(1); // Optional: Reload chunks
             }
             ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Bookmarks", "F5-F8", showBookmarks_)) {
                showBookmarks_ = !showBookmarks_;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Toggle Theme", "Ctrl+T")) {
                currentTheme_ = (currentTheme_ == Theme::Dark) ? Theme::Light : Theme::Dark;
                applyTheme(currentTheme_);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Performance")) {
                if (ImGui::MenuItem("Enable VSync", nullptr, ctx.vsyncEnabled)) {
                    ctx.vsyncEnabled = !ctx.vsyncEnabled;
                    ctx.needsRecreate = true;
                }
                if (ImGui::MenuItem("Limit FPS when Idle", nullptr, ctx.limitIdleFps)) {
                    ctx.limitIdleFps = !ctx.limitIdleFps;
                }
                if (ImGui::Checkbox("Cap FPS", &ctx.fpsCapEnabled)) {
                    // nothing else
                }
                ImGui::BeginDisabled(!ctx.fpsCapEnabled);
                float cap = ctx.fpsCapTarget;
                if (ImGui::SliderFloat("Target FPS", &cap, 30.0f, 1000.0f, "%.0f")) {
                    ctx.fpsCapTarget = cap;
                }
                ImGui::EndDisabled();
                // Voxel Terrain Settings Removed
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::Separator();
            if (ImGui::MenuItem("Generate Hydrology Report")) {
                if (ctx.finiteMap) {
                    // v3.7.8: Pass Resolution
                    bool success = terrain::HydrologyReport::generateToFile(*ctx.finiteMap, ctx.worldResolution, "hydrology_report.txt");
                    if (success) {
                        std::cout << "[UI] Hydrology Report generated successfully to 'hydrology_report.txt'" << std::endl;
                    } else {
                        std::cerr << "[UI] Failed to generate Hydrology Report." << std::endl;
                    }
                } else {
                     std::cerr << "[UI] No Finite Map available for report." << std::endl;
                }
            }

            if (ImGui::MenuItem("Generate Landscape Report (LSI/CF/RCC)")) {
                if (ctx.finiteMap) {
                    auto globalMetrics = terrain::LandscapeMetricCalculator::analyzeGlobal(*ctx.finiteMap, ctx.worldResolution);
                    auto basinMetrics = terrain::LandscapeMetricCalculator::analyzeByBasin(*ctx.finiteMap, ctx.worldResolution);

                    std::ofstream outFile("landscape_report.txt");
                    if (outFile.is_open()) {
                        outFile << terrain::LandscapeMetricCalculator::formatReport(globalMetrics, "GLOBAL LANDSCAPE METRICS");
                        outFile << "\n========================================\n\n";
                        for(const auto& pair : basinMetrics) {
                            int bid = pair.first;
                            const auto& m = pair.second;
                            outFile << terrain::LandscapeMetricCalculator::formatReport(m, "BASIN " + std::to_string(bid) + " METRICS");
                        }
                        outFile.close();
                        std::cout << "[UI] Landscape Report generated to 'landscape_report.txt'" << std::endl;
                    } else {
                        std::cerr << "[UI] Failed to open 'landscape_report.txt'" << std::endl;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Watershed Analysis (v3.6.3)")) {
                if (ImGui::MenuItem("Global Segmentation")) {
                    if (ctx.finiteMap) {
                        int count = terrain::Watershed::segmentGlobal(*ctx.finiteMap);
                        std::cout << "[UI] Global Watershed Segmentation complete. Basins: " << count << std::endl;
                        // Trigger renderer update
                        if (callbacks_.updateMesh) callbacks_.updateMesh();
                        ctx.showWatershedVis = true;
                        if (ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;
                        if (ctx.showDrainage) ctx.showDrainage = false;
                    }
                }
                ImGui::Separator();
                ImGui::TextDisabled("Interactive Mode:");
                ImGui::TextDisabled("Right-Click on map to Delineate Basin");
                // We need a state bit for "Interactive Watershed Mode", maybe ctx.showSlopeAnalysis is reused or new one?
                // Let's rely on Right Click logic in `Application` or here if we have input?
                // `UiLayer` draws UI. Input logic is usually elsewhere.
                // But we can toggle a flag "Show Watersheds" here.
                
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UiLayer::drawCamera(UiFrameContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

    ImGui::Begin("Camera Controls Panel");

    const char* modeName = (ctx.camera.getCameraMode() == graphics::CameraMode::Orbital)
                               ? "Orbital"
                               : (ctx.camera.isFlying() ? "Free Flight (Creative)" : "Free Flight (Walking)");
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Mode: %s", modeName);

    if (ctx.camera.getCameraMode() == graphics::CameraMode::FreeFlight) {
        bool flying = ctx.camera.isFlying();
        if (ImGui::Checkbox("Creative Mode (Fly)", &flying)) {
            ctx.camera.setFlying(flying);
        }
    }

    ImGui::Separator();

    if (ctx.camera.getCameraMode() == graphics::CameraMode::Orbital) {
        ImGui::Text("Controls:");
        ImGui::BulletText("Left Mouse: Orbit");
        ImGui::BulletText("Shift + Drag: Pan");
        ImGui::BulletText("Mouse Wheel: Zoom");
        ImGui::BulletText("Right Click: Pick Point");
    } else {
        ImGui::Text("Controls:");
        ImGui::BulletText("WASD: Move");
        ImGui::BulletText("Q/E: Down/Up (Fly Mode)");
        ImGui::BulletText("Z/X: Tilt/Level (Roll)");
        ImGui::BulletText("Space: Jump");
        ImGui::BulletText("Right Mouse + Drag: Look");
        ImGui::BulletText("Shift: Speed Boost");
        ImGui::BulletText("Alt: Slow Motion");
        ImGui::BulletText("[ / ]: Narrow/Wide FOV");

        float fovDeg = ctx.camera.getFovDegrees();
        if (ImGui::SliderFloat("FOV", &fovDeg, 45.0f, 110.0f, "%.1f deg")) {
            ctx.camera.setFovDegrees(fovDeg);
        }

        float rollDeg = ctx.camera.getRollDegrees();
        if (ImGui::SliderFloat("Tilt (Roll)", &rollDeg, -60.0f, 60.0f, "%.1f deg")) {
            ctx.camera.resetRoll();
            ctx.camera.addRoll(rollDeg);
        }

        if (ctx.lastSurfaceValid) {
            ImGui::Text("Surface: %s", ctx.lastSurfaceInfo.c_str());
        } else {
            ImGui::Text("Surface: -");
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Shortcuts:");
    ImGui::BulletText("Tab/C: Toggle Camera Mode");
    ImGui::BulletText("R: Reset to Origin");
    ImGui::BulletText("T: Teleport to Selected");
    ImGui::BulletText("1-4: Quick Teleports");

    ImGui::Separator();
    ImGui::Text("Quick Teleports:");
    if (ImGui::Button("Origin (0,0,0)")) {
        ctx.camera.teleportTo({0.0f, 2.0f, 5.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Quadrant 1")) {
        ctx.camera.teleportTo({10.0f, 2.0f, 10.0f});
    }
    if (ImGui::Button("Quadrant 2")) {
        ctx.camera.teleportTo({-10.0f, 2.0f, 10.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Quadrant 3")) {
        ctx.camera.teleportTo({-10.0f, 2.0f, -10.0f});
    }

    ImGui::End();
}

void UiLayer::drawAnimation(UiFrameContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(320, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

    ImGui::Begin("Animation Controls");

    if (ImGui::Checkbox("Enable Animation", &ctx.animationEnabled)) {
        if (ctx.animationEnabled) {
            ctx.axesAnimator.setAutoRotate(true, {0.0f, 1.0f, 0.0f}, 45.0f);
            std::cout << "[Animation] Enabled" << std::endl;
        } else {
            ctx.axesAnimator.setAutoRotate(false);
            std::cout << "[Animation] Disabled" << std::endl;
        }
    }

    if (ctx.animationEnabled) {
        ImGui::Separator();
        ImGui::Text("Axes Rotation:");

        static float speed = 45.0f;
        if (ImGui::SliderFloat("Speed (deg/s)", &speed, 10.0f, 180.0f)) {
            ctx.axesAnimator.setAutoRotate(true, {0.0f, 1.0f, 0.0f}, speed);
        }

        ImGui::Text("Rotation Axis:");
        if (ImGui::Button("X Axis")) {
            ctx.axesAnimator.setAutoRotate(true, {1.0f, 0.0f, 0.0f}, speed);
        }
        ImGui::SameLine();
        if (ImGui::Button("Y Axis")) {
            ctx.axesAnimator.setAutoRotate(true, {0.0f, 1.0f, 0.0f}, speed);
        }
        ImGui::SameLine();
        if (ImGui::Button("Z Axis")) {
            ctx.axesAnimator.setAutoRotate(true, {0.0f, 0.0f, 1.0f}, speed);
        }
    }

    ImGui::End();
}

void UiLayer::drawBookmarks(UiFrameContext& ctx) {
    if (!showBookmarks_) return;

    ImGui::SetNextWindowPos(ImVec2(610, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);

    ImGui::Begin("Bookmarks", &showBookmarks_);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Saved Positions");
    ImGui::Separator();

    static char bookmarkName[64] = "";
    ImGui::InputText("Name", bookmarkName, IM_ARRAYSIZE(bookmarkName));
    ImGui::SameLine();
    if (ImGui::Button("Save##BookmarkSave")) {
        if (callbacks_.saveBookmark) {
            callbacks_.saveBookmark(std::string(bookmarkName));
            bookmarkName[0] = '\0';
        }
    }

    ImGui::Separator();
    ImGui::Text("Bookmarks (%zu):", ctx.bookmarks.size());

    for (size_t i = 0; i < ctx.bookmarks.size(); ++i) {
        const auto& bm = ctx.bookmarks[i];

        ImGui::PushID(static_cast<int>(i));

        ImGui::Text("%s", bm.name.c_str());
        ImGui::SameLine(200);

        if (ImGui::SmallButton("Load")) {
            if (callbacks_.loadBookmark) {
                callbacks_.loadBookmark(i);
            }
        }
        ImGui::SameLine();

        if (ImGui::SmallButton("Del")) {
            if (callbacks_.deleteBookmark) {
                callbacks_.deleteBookmark(i);
            }
        }

        ImGui::PopID();
    }

    if (ctx.bookmarks.empty()) {
        ImGui::TextDisabled("No bookmarks saved");
        ImGui::TextDisabled("Press F5 to quick save");
    }

    ImGui::Separator();
    ImGui::TextDisabled("F5: Save | F6-F8: Load slots 1-3");

    ImGui::End();
}

void UiLayer::drawResetCamera(UiFrameContext& ctx) {
    float viewportW = ImGui::GetMainViewport()->Size.x;
    ImGui::SetNextWindowPos(ImVec2(viewportW - 130, 10));
    ImGui::SetNextWindowSize(ImVec2(120, 0));
    ImGui::Begin("CamControls", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Reset Camera")) {
        ctx.camera.reset();
    }
    ImGui::End();
}

void UiLayer::applyTheme(Theme theme) {
    currentTheme_ = theme;
    ImGuiStyle& style = ImGui::GetStyle();

    if (theme == Theme::Dark) {
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.15f, 0.94f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.08f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.0f);
        std::cout << "[Theme] Applied Dark theme" << std::endl;
    } else {
        ImGui::StyleColorsLight();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.8f, 0.8f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.5f, 0.6f, 0.8f, 1.0f);
        std::cout << "[Theme] Applied Light theme" << std::endl;
    }
}



void UiLayer::drawToolbar(UiFrameContext& ctx) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float menuHeight = 18.0f; // Approx height of main menu
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 50));
    // ImGui::SetNextWindowViewport(viewport->ID); // Not available in this version

    // Transparent / Toolbar style?
    // Let's make it a standard window for now
    
    if (ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse)) {
        
        // Style for buttons
        ImVec2 btnSize(120, 30);
        
        // Terrain Mode
        bool isTerrain = (activeTool_ == ActiveTool::Terrain);
        if (isTerrain) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Terrain & Env", btnSize)) activeTool_ = ActiveTool::Terrain;
        if (isTerrain) ImGui::PopStyleColor();

        ImGui::SameLine();

        // Hydro Mode
        bool isHydro = (activeTool_ == ActiveTool::Hydro);
        if (isHydro) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.8f, 1.0f));
        if (ImGui::Button("Hydrology", btnSize)) activeTool_ = ActiveTool::Hydro;
        if (isHydro) ImGui::PopStyleColor();

        ImGui::SameLine();
        
        // Soil Mode
        bool isSoil = (activeTool_ == ActiveTool::Soil);
        if (isSoil) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
        if (ImGui::Button("Soil", btnSize)) activeTool_ = ActiveTool::Soil;
        if (isSoil) ImGui::PopStyleColor();

        ImGui::SameLine();
        
        // Veg Mode
        bool isVeg = (activeTool_ == ActiveTool::Vegetation);
        if (isVeg) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        if (ImGui::Button("Vegetation", btnSize)) activeTool_ = ActiveTool::Vegetation;
        if (isVeg) ImGui::PopStyleColor();
        
        ImGui::SameLine();

        // ML Hub Mode
        bool isML = (activeTool_ == ActiveTool::MLHub);
        if (isML) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.8f, 1.0f)); // Purple
        if (ImGui::Button("ML Service", btnSize)) activeTool_ = ActiveTool::MLHub;
        if (isML) ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Quick Actions
        if (ImGui::Button("Regenerate (Ctrl+G)")) {
             std::cout << "[UI] Toolbar Regenerate Clicked!" << std::endl;
             if (callbacks_.regenerateFiniteWorld && !ctx.isRegenerating) {
                 terrain::TerrainConfig config;
                 config.width = genSelectedSize_;
                 config.height = genSelectedSize_;
                 config.noiseScale = genScale_;
                 config.maxHeight = genAmplitude_;
                 config.resolution = genResolution_;
                 config.persistence = genPersistence_;
                 config.seed = genSeedInput_;
                 config.waterLevel = genWaterLvl_;
                 
                 if (genUseBlend_) {
                     config.model = terrain::TerrainConfig::FiniteTerrainModel::ExperimentalBlend;
                     config.blendConfig.lowFreqWeight = genBlendLow_;
                     config.blendConfig.midFreqWeight = genBlendMid_;
                     config.blendConfig.highFreqWeight = genBlendHigh_;
                     config.blendConfig.exponent = genBlendExp_;
                 } else {
                      config.model = terrain::TerrainConfig::FiniteTerrainModel::Default;
                 }
                 lastMetrics_.clear();
                 callbacks_.regenerateFiniteWorld(config);
             }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reset Camera")) ctx.camera.reset();

    }
    ImGui::End();
}

void UiLayer::drawInspector(UiFrameContext& ctx) {
    if (activeTool_ == ActiveTool::None) return;

    // Inspector Panel on Right Side
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float width = 350.0f;
    float topOffset = 70.0f; // Menu + Toolbar
    
    // Use FirstUseEver so user can move/resize it after initial launch
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - width, viewport->Pos.y + topOffset), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width, viewport->Size.y - topOffset), ImGuiCond_FirstUseEver);
    
    // Title based on mode
    const char* title = "Inspector";
    if (activeTool_ == ActiveTool::Terrain) title = "Terrain Inspector";
    if (activeTool_ == ActiveTool::Hydro) title = "Hydrology Inspector";
    if (activeTool_ == ActiveTool::Soil) title = "Pedology Inspector";
    if (activeTool_ == ActiveTool::Vegetation) title = "Vegetation Inspector";
    if (activeTool_ == ActiveTool::MLHub) title = "Machine Learning Hub";

    // Allow resizing and moving (Removed NoResize | NoMove)
    if (ImGui::Begin(title, nullptr)) {
        
        switch (activeTool_) {
            case ActiveTool::Terrain:
                drawTerrainInspector(ctx);
                break;
            case ActiveTool::Hydro:
                drawHydrologyInspector(ctx);
                break;
            case ActiveTool::Soil:
                drawSoilInspector(ctx);
                break;
            case ActiveTool::Vegetation:
                drawVegetationInspector(ctx);
                break;
            case ActiveTool::MLHub:
                drawMLInspector(ctx);
                break;
            default: break;
        }
        
    }
    ImGui::End();
}

void UiLayer::drawTerrainInspector(UiFrameContext& ctx) {
    // --- Visual Settings ---
    if (ImGui::CollapsingHeader("Environment & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::SliderFloat("Sun Azimuth", &ctx.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
         ImGui::SliderFloat("Sun Elevation", &ctx.sunElevation, -90.0f, 90.0f, "%.0f deg");
         ImGui::SliderFloat("Light Intensity", &ctx.lightIntensity, 0.0f, 2.0f, "%.2f");
         ImGui::SliderFloat("Render Distance", &ctx.fogDensity, 0.0f, 0.005f, "%.5f");
    }

    // --- Analysis Tools ---
    ImGui::Separator();
    // --- Analysis Tools ---
    ImGui::Separator();
    
    // 1. Slope Analysis
    if (ImGui::Checkbox("Show Slope Analysis", &ctx.showSlopeAnalysis)) {
        if (ctx.showSlopeAnalysis) {
             ctx.showDrainage = false;
             ctx.showWatershedVis = false;
             ctx.showSoilVis = false;
        }
    }
    
    // Legend for Slope
    if (ctx.showSlopeAnalysis) {
         ImGui::Indent();
         ImGui::TextColored(ImVec4(0.2f,0.4f,0.8f,1.0f), "0-3%%: Flat");
         ImGui::TextColored(ImVec4(0.2f,0.8f,0.8f,1.0f), "3-8%%: Gentle");
         ImGui::TextColored(ImVec4(0.2f,0.8f,0.2f,1.0f), "8-20%%: Moderate");
         ImGui::TextColored(ImVec4(0.8f,0.8f,0.0f,1.0f), "20-45%%: Steep");
         ImGui::TextColored(ImVec4(1.0f,0.5f,0.0f,1.0f), "45-75%%: V.Steep");
         ImGui::TextColored(ImVec4(0.8f,0.0f,0.0f,1.0f), ">75%%: Extreme");
         ImGui::Unindent();
    }

    // 2. SCORPAN Model (only supported visualization)
    bool showScorpan = ctx.showSoilVis && (ctx.soilClassificationMode >= 1);
    if (ImGui::Checkbox("SCORPAN Model (Simulation)", &showScorpan)) {
        ctx.showSoilVis = showScorpan;
        if (showScorpan) {
             ctx.soilClassificationMode = 1;
             if (callbacks_.switchSoilMode) callbacks_.switchSoilMode(1);

             // Mutual Exclusion
             ctx.showSlopeAnalysis = false;
             ctx.showDrainage = false;
             ctx.showWatershedVis = false;
         }
    }

    // --- Generation Config ---
    ImGui::Separator();
    ImGui::Text("Generation Parameters:");

    // Generation Config (Refactored to Members v4.3.2)
    ImGui::Separator();
    ImGui::Text("Generation Parameters:");

    if (ImGui::Combo("Terrain Preset", &genPreset_, "Plains\0Hills\0Mountains\0Alpine\0\0")) {
        if (genPreset_ == 0) { genScale_ = 0.001f; genAmplitude_ = 40.0f; genPersistence_ = 0.4f; genWaterLvl_ = 30.0f; }      
        if (genPreset_ == 1) { genScale_ = 0.002f; genAmplitude_ = 80.0f; genPersistence_ = 0.5f; genWaterLvl_ = 64.0f; }      
        if (genPreset_ == 2) { genScale_ = 0.003f; genAmplitude_ = 180.0f; genPersistence_ = 0.6f; genWaterLvl_ = 80.0f; }     
        if (genPreset_ == 3) { genScale_ = 0.004f; genAmplitude_ = 250.0f; genPersistence_ = 0.7f; genWaterLvl_ = 100.0f; }     
    }

    ImGui::SliderFloat("Feature Size", &genScale_, 0.0001f, 0.01f, "%.4f");
    ImGui::SliderFloat("Roughness", &genPersistence_, 0.2f, 0.8f, "%.2f");
    ImGui::SliderFloat("Amplitude", &genAmplitude_, 50.0f, 500.0f, "%.0f m");
    ImGui::SliderFloat("Water Level", &genWaterLvl_, 0.0f, 200.0f, "%.0f m");

    // Map Size
    const char* sizeItems[] = { "512 x 512", "1024 x 1024", "2048 x 2048", "4096 x 4096" };
    int currentSizeIdx = 1; 
    if (genSelectedSize_ == 512) currentSizeIdx = 0;
    else if (genSelectedSize_ == 1024) currentSizeIdx = 1;
    else if (genSelectedSize_ == 2048) currentSizeIdx = 2;
    else if (genSelectedSize_ == 4096) currentSizeIdx = 3;
    if (ImGui::Combo("Map Size", &currentSizeIdx, sizeItems, IM_ARRAYSIZE(sizeItems))) {
        if (currentSizeIdx == 0) genSelectedSize_ = 512;
        if (currentSizeIdx == 1) genSelectedSize_ = 1024;
        if (currentSizeIdx == 2) genSelectedSize_ = 2048;
        if (currentSizeIdx == 3) genSelectedSize_ = 4096;
    }

    ImGui::SliderFloat("Resolution", &genResolution_, 0.1f, 4.0f, "%.1f m");
    
    ImGui::Checkbox("Use Experimental Blend", &genUseBlend_);
    if (genUseBlend_) {
        ImGui::Indent();
        ImGui::SliderFloat("Low Freq", &genBlendLow_, 0.0f, 2.0f);
        ImGui::SliderFloat("Mid Freq", &genBlendMid_, 0.0f, 2.0f);
        ImGui::SliderFloat("High Freq", &genBlendHigh_, 0.0f, 1.0f);
        ImGui::SliderFloat("Exponent", &genBlendExp_, 0.1f, 4.0f);
        ImGui::Unindent();
    }

    static bool initialized = false;
    if (!initialized) { genSeedInput_ = ctx.seed; initialized = true; }
    ImGui::InputInt("Seed", &genSeedInput_);
    ImGui::SameLine();
    if (ImGui::Button("Rnd")) genSeedInput_ = rand(); 

    ImGui::Separator();
    
    if (ctx.isRegenerating) ImGui::BeginDisabled();
    bool gen = ImGui::Button("Generate Map (Ctrl+G)", ImVec2(-1, 40)) || (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_G, false));
    if (gen) {
        std::cout << "[UI] Generate Button Clicked!" << std::endl;
        if (callbacks_.regenerateFiniteWorld) {
            terrain::TerrainConfig config;
            config.width = genSelectedSize_;
            config.height = genSelectedSize_;
            config.noiseScale = genScale_;
            config.maxHeight = genAmplitude_;
            config.resolution = genResolution_;
            config.persistence = genPersistence_;
            config.seed = genSeedInput_;
            config.waterLevel = genWaterLvl_;
            if (genUseBlend_) {
                config.model = terrain::TerrainConfig::FiniteTerrainModel::ExperimentalBlend;
                config.blendConfig.lowFreqWeight = genBlendLow_;
                config.blendConfig.midFreqWeight = genBlendMid_;
                config.blendConfig.highFreqWeight = genBlendHigh_;
                config.blendConfig.exponent = genBlendExp_;
            } else {
                 config.model = terrain::TerrainConfig::FiniteTerrainModel::Default;
            }
            // v4.3.5: Force clear metrics to avoid stale report
            lastMetrics_.clear();
            
            callbacks_.regenerateFiniteWorld(config);
        }
    }
    if (ctx.isRegenerating) ImGui::EndDisabled();

    // --- Navigation ---
    ImGui::Separator();
    ImGui::Text("Viewer Controls:");
    float currentSpeed = ctx.camera.getMoveSpeed();
    if (ImGui::SliderFloat("Fly Speed", &currentSpeed, 10.0f, 200.0f, "%.0f m/s")) {
        ctx.camera.setMoveSpeed(currentSpeed);
    }

    if (ImGui::Button("Orbit Map", ImVec2(100, 0))) {
        if (ctx.finiteMap) {
            float cx = ctx.finiteMap->getWidth() / 2.0f;
            float cz = ctx.finiteMap->getHeight() / 2.0f;
            ctx.camera.setTarget({cx, 40.0f, cz});
            ctx.camera.setCameraMode(graphics::CameraMode::Orbital);
            ctx.camera.setDistance(ctx.finiteMap->getWidth() * 0.8f); 
            ctx.camera.setFarClip(500.0f); 
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fly Center", ImVec2(100, 0))) {
        if (ctx.finiteMap) {
            float cx = ctx.finiteMap->getWidth() / 2.0f;
            float cz = ctx.finiteMap->getHeight() / 2.0f;
            float h = ctx.finiteMap->getHeight(static_cast<int>(cx), static_cast<int>(cz));
            ctx.camera.teleportTo({cx, h + 50.0f, cz});
            ctx.camera.setCameraMode(graphics::CameraMode::FreeFlight);
            ctx.camera.setFarClip(500.0f); 
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Top View", ImVec2(100, 0))) {
        if (minimap_) {
            float w = minimap_->getWorldWidth();
            float h = minimap_->getWorldHeight();
            float cx = w / 2.0f;
            float cz = h / 2.0f;
            float alt = std::max(w, h); 
            ctx.camera.setCameraMode(graphics::CameraMode::FreeFlight);
            ctx.camera.setFlying(true);
            ctx.camera.teleportTo({cx, alt, cz});
            ctx.camera.setPitch(-89.9f); 
            ctx.camera.setYaw(0.0f);   
            ctx.fogDensity = 0.0f; 
            ctx.camera.setFarClip(8000.0f); 
        }
    }
}

void UiLayer::drawHydrologyInspector(UiFrameContext& ctx) {
    ImGui::SliderFloat("Rain Intensity", &ctx.rainIntensity, 0.0f, 100.0f, "%.1f mm/h");
    
    ImGui::Separator();
    ImGui::Checkbox("Show Drainage (Flux)", &ctx.showDrainage);
    if (ctx.showDrainage) {
            ImGui::Indent();
            ImGui::SliderFloat("Viz Threshold", &ctx.drainageIntensity, 0.05f, 1.0f);
            ImGui::Unindent();
            // Exclusions
            if (ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;
            if (ctx.showWatershedVis) ctx.showWatershedVis = false;
    }

    ImGui::Separator();
    bool wasShown = ctx.showWatershedVis;
    ImGui::Checkbox("Show Watersheds (Basins)", &ctx.showWatershedVis);
    if (ctx.showWatershedVis) {
            ImGui::Indent();
            ImGui::Checkbox("Show Contours", &ctx.showBasinOutlines);
            ImGui::Unindent();

            if (!wasShown && ctx.finiteMap) {
                terrain::Watershed::segmentGlobal(*ctx.finiteMap);
                if (callbacks_.updateMesh) callbacks_.updateMesh();
            }
            // Exclusions
            if (ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;
            if (ctx.showDrainage) ctx.showDrainage = false;
    }
}

// drawGeologyInspector removed - merged into Soil Inspector v4.5.1


void UiLayer::drawSoilInspector(UiFrameContext& ctx) {

    // v4.5.1: Unified SCORPAN Interface + v4.6.0 Domain
    ImGui::TextColored(ImVec4(0.7f, 0.5f, 0.3f, 1.0f), "Soil System (SCORPAN + SiBCS)");
    
    // --- Domain Definition (v4.6.0) ---
    if (ctx.sibcsConfig && ImGui::CollapsingHeader("Domain Definition (User)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& cfg = *ctx.sibcsConfig;
        auto markDirty = [&](landscape::SiBCSUserConfig& c) {
            c.pendingChanges = true;
            c.domainConfirmed = false;
        };

        auto syncOrders = [&](landscape::SiBCSUserConfig& c) {
            c.allowedOrders.clear();
            for (const auto& sel : c.selections) {
                if (sel.order == landscape::SiBCSOrder::kNone) continue;
                if (std::find(c.allowedOrders.begin(), c.allowedOrders.end(), sel.order) == c.allowedOrders.end()) {
                    c.allowedOrders.push_back(sel.order);
                }
            }
        };

        auto orderName = [](landscape::SiBCSOrder o)->const char* {
            switch(o) {
                case landscape::SiBCSOrder::kLatossolo: return "Latossolo";
                case landscape::SiBCSOrder::kArgissolo: return "Argissolo";
                case landscape::SiBCSOrder::kCambissolo: return "Cambissolo";
                case landscape::SiBCSOrder::kNeossoloLit: return "Neossolo (Litolico)";
                case landscape::SiBCSOrder::kNeossoloQuartz: return "Neossolo (Quartzarenico)";
                case landscape::SiBCSOrder::kGleissolo: return "Gleissolo";
                case landscape::SiBCSOrder::kOrganossolo: return "Organossolo";
                case landscape::SiBCSOrder::kPlintossolo: return "Plintossolo";
                case landscape::SiBCSOrder::kEspodossolo: return "Espodossolo";
                case landscape::SiBCSOrder::kVertissolo: return "Vertissolo";
                case landscape::SiBCSOrder::kPlanossolo: return "Planossolo";
                case landscape::SiBCSOrder::kChernossolo: return "Chernossolo";
                case landscape::SiBCSOrder::kNitossolo: return "Nitossolo";
                case landscape::SiBCSOrder::kLuvissolo: return "Luvissolo";
                default: return "(Nenhum)";
            }
        };

        ImGui::Checkbox("Enforce Custom Domain", &cfg.applyConstraints);
        if (ImGui::IsItemEdited()) {
            if (!cfg.applyConstraints) {
                cfg.domainConfirmed = false;
                cfg.pendingChanges = false;
            } else {
                markDirty(cfg);
            }
        }

        if (cfg.pendingChanges) {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Pending changes - soil simulation paused until Apply.");
        } else if (cfg.domainConfirmed) {
            ImGui::TextDisabled("Domain confirmed. Simulation allowed.");
        } else {
            ImGui::TextDisabled("Domain not confirmed.");
        }
        
        if (cfg.applyConstraints) {
            ImGui::Indent();
            ImGui::TextDisabled("Select Orders (Level 1):");
            
            auto OrderCheckbox = [&](const char* label, landscape::SiBCSOrder order) {
                bool active = std::any_of(cfg.selections.begin(), cfg.selections.end(), [&](const landscape::SiBCSUserSelection& sel) {
                    return sel.order == order;
                });
                if (ImGui::Checkbox(label, &active)) {
                    if (active) {
                        landscape::SiBCSUserSelection sel;
                        sel.order = order;
                        cfg.selections.push_back(sel);
                    } else {
                        cfg.selections.erase(std::remove_if(cfg.selections.begin(), cfg.selections.end(), [&](const landscape::SiBCSUserSelection& sel) {
                            return sel.order == order;
                        }), cfg.selections.end());
                    }
                    syncOrders(cfg);
                    markDirty(cfg);
                }
            };
            
            ImGui::Columns(2, "soil_cols", false);
            OrderCheckbox("Latossolo", landscape::SiBCSOrder::kLatossolo);
            OrderCheckbox("Argissolo", landscape::SiBCSOrder::kArgissolo);
            OrderCheckbox("Cambissolo", landscape::SiBCSOrder::kCambissolo);
            ImGui::NextColumn();
            OrderCheckbox("Neossolo", landscape::SiBCSOrder::kNeossoloLit);
            OrderCheckbox("Gleissolo", landscape::SiBCSOrder::kGleissolo);
            OrderCheckbox("Organossolo", landscape::SiBCSOrder::kOrganossolo);
            ImGui::Columns(1);

            ImGui::Spacing();
            ImGui::TextDisabled("Explicit SiBCS selection (no automatic combinations):");

            // Selection builders (combos)
            static int orderIdx = 0;
            static int subIdx = 0;
            static int groupIdx = 0;
            static int subGroupIdx = 0;

            const std::pair<const char*, landscape::SiBCSOrder> orderOpts[] = {
                {"(Nenhum)", landscape::SiBCSOrder::kNone},
                {"Latossolo", landscape::SiBCSOrder::kLatossolo},
                {"Argissolo", landscape::SiBCSOrder::kArgissolo},
                {"Cambissolo", landscape::SiBCSOrder::kCambissolo},
                {"Neossolo (Litolico)", landscape::SiBCSOrder::kNeossoloLit},
                {"Neossolo (Quartzarenico)", landscape::SiBCSOrder::kNeossoloQuartz},
                {"Gleissolo", landscape::SiBCSOrder::kGleissolo},
                {"Organossolo", landscape::SiBCSOrder::kOrganossolo},
                {"Plintossolo", landscape::SiBCSOrder::kPlintossolo},
                {"Espodossolo", landscape::SiBCSOrder::kEspodossolo},
                {"Vertissolo", landscape::SiBCSOrder::kVertissolo},
                {"Planossolo", landscape::SiBCSOrder::kPlanossolo},
                {"Chernossolo", landscape::SiBCSOrder::kChernossolo},
                {"Nitossolo", landscape::SiBCSOrder::kNitossolo},
                {"Luvissolo", landscape::SiBCSOrder::kLuvissolo}
            };

            const std::pair<const char*, landscape::SiBCSSubOrder> subOpts[] = {
                {"(Nenhum)", landscape::SiBCSSubOrder::kNone},
                {"Vermelho", landscape::SiBCSSubOrder::kVermelho},
                {"Amarelo", landscape::SiBCSSubOrder::kAmarelo},
                {"Vermelho-Amarelo", landscape::SiBCSSubOrder::kVermelhoAmarelo},
                {"Bruno", landscape::SiBCSSubOrder::kBruno},
                {"Haplico", landscape::SiBCSSubOrder::kHaplic},
                {"Litolico", landscape::SiBCSSubOrder::kLitolico},
                {"Regolitico", landscape::SiBCSSubOrder::kRegolitico},
                {"Fluvico", landscape::SiBCSSubOrder::kFluvico},
                {"Quartzarenico", landscape::SiBCSSubOrder::kQuartzarenico},
                {"Melanico", landscape::SiBCSSubOrder::kMelanico},
                {"Tiomorfico", landscape::SiBCSSubOrder::kTiomorfico},
                {"Salico", landscape::SiBCSSubOrder::kSalico},
                {"Humico", landscape::SiBCSSubOrder::kHumico},
                {"Gleico", landscape::SiBCSSubOrder::kGleico}
            };

            const std::pair<const char*, landscape::SiBCSGreatGroup> groupOpts[] = {
                {"(Nenhum)", landscape::SiBCSGreatGroup::kNone},
                {"Eutrofico", landscape::SiBCSGreatGroup::kEutrofico},
                {"Distrofico", landscape::SiBCSGreatGroup::kDistrofico},
                {"Tb Eutrofico", landscape::SiBCSGreatGroup::kTbEutrofico},
                {"Tb Distrofico", landscape::SiBCSGreatGroup::kTbDistrofico},
                {"Aluminico", landscape::SiBCSGreatGroup::kAluminico},
                {"Acrico", landscape::SiBCSGreatGroup::kAcrico},
                {"Ferrico", landscape::SiBCSGreatGroup::kFerrico},
                {"Distroferrico", landscape::SiBCSGreatGroup::kDistroferrico},
                {"Ortico", landscape::SiBCSGreatGroup::kOrtico},
                {"Hidromorfico", landscape::SiBCSGreatGroup::kHidromorfico},
                {"Tipico", landscape::SiBCSGreatGroup::kTipico}
            };

            const std::pair<const char*, landscape::SiBCSSubGroup> subGroupOpts[] = {
                {"(Nenhum)", landscape::SiBCSSubGroup::kNone},
                {"Tipico", landscape::SiBCSSubGroup::kTipico},
                {"Latossolico", landscape::SiBCSSubGroup::kLatossolico},
                {"Argissolico", landscape::SiBCSSubGroup::kArgissolico},
                {"Cambissolico", landscape::SiBCSSubGroup::kCambissolico},
                {"Psamitico", landscape::SiBCSSubGroup::kPsamitico},
                {"Humico", landscape::SiBCSSubGroup::kHumico},
                {"Petroplintico", landscape::SiBCSSubGroup::kPetroplintico},
                {"Salico", landscape::SiBCSSubGroup::kSalico},
                {"Tiomorfico", landscape::SiBCSSubGroup::kTiomorfico}
            };

            ImGui::Combo("Ordem", &orderIdx, [](void* data, int idx, const char** out_text){
                auto* opts = static_cast<const std::pair<const char*, landscape::SiBCSOrder>*>(data);
                *out_text = opts[idx].first;
                return true;
            }, (void*)orderOpts, IM_ARRAYSIZE(orderOpts));

            ImGui::Combo("Subordem", &subIdx, [](void* data, int idx, const char** out_text){
                auto* opts = static_cast<const std::pair<const char*, landscape::SiBCSSubOrder>*>(data);
                *out_text = opts[idx].first;
                return true;
            }, (void*)subOpts, IM_ARRAYSIZE(subOpts));

            ImGui::Combo("Grande Grupo", &groupIdx, [](void* data, int idx, const char** out_text){
                auto* opts = static_cast<const std::pair<const char*, landscape::SiBCSGreatGroup>*>(data);
                *out_text = opts[idx].first;
                return true;
            }, (void*)groupOpts, IM_ARRAYSIZE(groupOpts));

            ImGui::Combo("Subgrupo", &subGroupIdx, [](void* data, int idx, const char** out_text){
                auto* opts = static_cast<const std::pair<const char*, landscape::SiBCSSubGroup>*>(data);
                *out_text = opts[idx].first;
                return true;
            }, (void*)subGroupOpts, IM_ARRAYSIZE(subGroupOpts));

            if (ImGui::Button("Add Selection")) {
                landscape::SiBCSUserSelection sel;
                sel.order = orderOpts[orderIdx].second;
                sel.suborder = subOpts[subIdx].second;
                sel.greatGroup = groupOpts[groupIdx].second;
                sel.subGroup = subGroupOpts[subGroupIdx].second;

                if (sel.order != landscape::SiBCSOrder::kNone) {
                    bool exists = std::any_of(cfg.selections.begin(), cfg.selections.end(), [&](const landscape::SiBCSUserSelection& existing) {
                        return existing.order == sel.order &&
                               existing.suborder == sel.suborder &&
                               existing.greatGroup == sel.greatGroup &&
                               existing.subGroup == sel.subGroup;
                    });
                    if (!exists) {
                        cfg.selections.push_back(sel);
                        syncOrders(cfg);
                        markDirty(cfg);
                    }
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Current selections:");
            if (cfg.selections.empty()) {
                ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.2f, 1.0f), "None. Simulation will remain passive.");
            } else {
                for (size_t i = 0; i < cfg.selections.size(); ++i) {
                    const auto& sel = cfg.selections[i];
                    ImGui::BulletText("%s", orderName(sel.order));
                    ImGui::SameLine();
                    ImGui::TextDisabled("| Sub: %d | GG: %d | SG: %d", static_cast<int>(sel.suborder), static_cast<int>(sel.greatGroup), static_cast<int>(sel.subGroup));
                    ImGui::SameLine();
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::SmallButton("Remove")) {
                        cfg.selections.erase(cfg.selections.begin() + static_cast<long>(i));
                        syncOrders(cfg);
                        markDirty(cfg);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::Button("Clear All")) {
                    cfg.selections.clear();
                    syncOrders(cfg);
                    markDirty(cfg);
                }
            }

            ImGui::Unindent();
        }
        
        ImGui::Dummy(ImVec2(0,5));
        bool noDomain = (!cfg.applyConstraints) || (cfg.selections.empty() && cfg.allowedOrders.empty());
        ImGui::BeginDisabled(noDomain);
        if (ImGui::Button("Apply & Regenerate", ImVec2(-1, 0))) {
            cfg.domainConfirmed = true;
            cfg.pendingChanges = false;
            if (callbacks_.recomputeSoil) callbacks_.recomputeSoil();
        }
        ImGui::EndDisabled();
        ImGui::Separator();
    }
    
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Recalculate Soil Initial State", ImVec2(-1, 0))) {
        if (ctx.sibcsConfig && ctx.sibcsConfig->applyConstraints) {
            ctx.sibcsConfig->domainConfirmed = true;
            ctx.sibcsConfig->pendingChanges = false;
        }
        if (callbacks_.recomputeSoil) {
            callbacks_.recomputeSoil();
        }
    }
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Separator();
    ImGui::Text("Visualization (SiBCS domain only):");
    
    bool showSco = ctx.showSoilVis && (ctx.soilClassificationMode >= 1);
    if (ImGui::RadioButton("Off", !showSco)) {
        ctx.showSoilVis = false; 
        ctx.soilClassificationMode = 1; // SCORPAN is the only supported mode
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("SCORPAN (SiBCS)", showSco)) {
        ctx.showSoilVis = true;
        ctx.soilClassificationMode = 1;
        if (callbacks_.switchSoilMode) callbacks_.switchSoilMode(1);
        ctx.showSlopeAnalysis = false;
        ctx.showDrainage = false;
        ctx.showWatershedVis = false;
    }
    
    ImGui::TextDisabled(showSco ? "Visualization uses only the user-confirmed SiBCS domain." : "Visualization disabled.");
    // Force SCORPAN depth to Order-only view (no level switching UI)
    if (ctx.soilClassificationMode < 1) ctx.soilClassificationMode = 1;
    if (ctx.soilClassificationMode > 1) ctx.soilClassificationMode = 1;
    
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Factors (Inputs / Loaded Data)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Factor C (Climate)
        ImGui::PushID("FactorC");
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "[C] Climate");
        float rain = static_cast<float>(ctx.soilClimate.rain_intensity);
        if (ImGui::SliderFloat("Rain Intensity", &rain, 0.0f, 1.0f)) {
            ctx.soilClimate.rain_intensity = rain;
            ctx.rainIntensity = rain * 100.0f; 
        }
        float seas = static_cast<float>(ctx.soilClimate.seasonality);
        if (ImGui::SliderFloat("Seasonality", &seas, 0.0f, 1.0f)) ctx.soilClimate.seasonality = seas;
        ImGui::PopID();

        ImGui::Separator();

        // Factor O (Organisms)
        ImGui::PushID("FactorO");
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[O] Organisms");
        float cover = static_cast<float>(ctx.soilOrganism.max_cover);
        if (ImGui::SliderFloat("Potential Cover", &cover, 0.0f, 1.0f)) ctx.soilOrganism.max_cover = cover;
        float dist = static_cast<float>(ctx.soilOrganism.disturbance);
        if (ImGui::SliderFloat("Disturbance", &dist, 0.0f, 1.0f)) ctx.soilOrganism.disturbance = dist;
        ImGui::PopID();

        ImGui::Separator();

        // Factor P (Parent Material)
        ImGui::PushID("FactorP");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "[P] Parent Material");
        const double min = 0.0;
        const double max = 1.0;
        ImGui::SliderScalar("Weathering Rate", ImGuiDataType_Double, &ctx.soilParentMaterial.weathering_rate, &min, &max, "%.3f");
        ImGui::SliderScalar("Base Fertility", ImGuiDataType_Double, &ctx.soilParentMaterial.base_fertility, &min, &max, "%.3f");
        ImGui::SliderScalar("Sand Bias", ImGuiDataType_Double, &ctx.soilParentMaterial.sand_bias, &min, &max, "%.3f");
        ImGui::SliderScalar("Clay Bias", ImGuiDataType_Double, &ctx.soilParentMaterial.clay_bias, &min, &max, "%.3f");
        ImGui::PopID();

        ImGui::Separator();

        // Factor R (Relief)
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "[R] Relief");
        ImGui::TextWrapped("Derived from curvature/slope. Drives erosion/deposition.");
        
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.8f, 1.0f), "Emergent State (S - Calculated)");
    ImGui::TextWrapped("System state evolves continuously relative to factors.");
    ImGui::BulletText("Physical: Depth, Sand/Clay fractions");
    ImGui::BulletText("Chemical: Organic Carbon content");
    ImGui::BulletText("Hydric: Water Storage, Field Capacity");
    
    ImGui::Separator();

    // --- Soil Map (SCORPAN only) ---
    if (ctx.showSoilVis && ctx.soilClassificationMode >= 1) {
        ImGui::Text("Map Legend (SiBCS Order)");
        ImGui::Indent();
        auto LegendItem = [](const char* name, terrain::SoilType type) {
            float c[3];
            terrain::SoilPalette::getFloatColor(type, c);
            ImGui::ColorButton(name, ImVec4(c[0], c[1], c[2], 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
            ImGui::SameLine(); ImGui::Text("%s", name);
        };
        LegendItem("Latossolo", terrain::SoilType::Latossolo);
        LegendItem("Argissolo", terrain::SoilType::Argissolo);
        LegendItem("Cambissolo", terrain::SoilType::Cambissolo);
        LegendItem("Neossolo", terrain::SoilType::Neossolo_Litolico);
        LegendItem("Gleissolo", terrain::SoilType::Gleissolo);
        LegendItem("Organossolo", terrain::SoilType::Organossolo);
        ImGui::Unindent();

        if (ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;
    }
    
    ImGui::Separator();

    // ML Hub Moved to drawMLInspector
}

// New Dedicated ML Inspector (v4.3.9)
void UiLayer::drawMLInspector(UiFrameContext& ctx) {
    ImGui::TextWrapped("The ML Service acts as a central intelligence hub connecting Soil, Hydro, and Vegetation systems.");
    ImGui::Separator();

    // Global Status
    if (ctx.isTraining) {
        ImGui::TextColored(ImVec4(1.0f,1.0f,0.0f,1.0f), "STATUS: TRAINING IN PROGRESS...");
        ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(0.0f,0.0f)); // Indeterminate animation
    } else {
        ImGui::TextColored(ImVec4(0.2f,1.0f,0.2f,1.0f), "STATUS: IDLE (Ready)");
    }
    
    ImGui::Separator();
    ImGui::Text("Hyperparameters (Shared):");
    ImGui::PushItemWidth(120);
    ImGui::InputInt("Epochs", &ctx.mlTrainingEpochs); 
    ImGui::InputFloat("Learning Rate", &ctx.mlLearningRate, 0.001f, 0.01f, "%.4f"); 
    ImGui::InputInt("Sample Batch", &ctx.mlSampleCount);
    ImGui::PopItemWidth();
    
    ImGui::Separator();
    ImGui::Text("Model Registry:");

	    if (ImGui::CollapsingHeader("1. Soil Color (Pedology)", ImGuiTreeNodeFlags_DefaultOpen)) {
	        ImGui::Checkbox("Use ML for Visualization", &ctx.showMLSoil);
	        ImGui::SameLine();
	        ImGui::TextDisabled("(Takes over texture generation)");
	
	        if (ctx.showMLSoil) {
	            ImGui::TextDisabled("Legend (ML Gradient):");
	            auto legendSwatch = [](const char* id, float r, float g, float b, const char* label) {
	                ImGui::ColorButton(id, ImVec4(r, g, b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
	                ImGui::SameLine();
	                ImGui::TextUnformatted(label);
	            };
	            legendSwatch("##mlLow", 0.0f, 0.0f, 1.0f, "Low (blue)");
	            legendSwatch("##mlMid", 1.0f, 1.0f, 0.0f, "Mid (yellow)");
	            legendSwatch("##mlHigh", 1.0f, 0.0f, 0.0f, "High (red)");
	        }
	        
	        ImGui::Text("Dataset: %zu samples", ctx.mlDatasetSize);
	        if (ImGui::Button("Collect Samples##Soil")) callbacks_.mlCollectData(ctx.mlSampleCount);
	        ImGui::SameLine();
        if (ctx.mlDatasetSize > 0) {
            if (ImGui::Button("Train Model##Soil")) callbacks_.mlTrainModel(ctx.mlTrainingEpochs, ctx.mlLearningRate);
        } else {
             ImGui::BeginDisabled();
             ImGui::Button("Train Model##Soil");
             ImGui::EndDisabled();
        }
    }

    if (ImGui::CollapsingHeader("2. Hydro Runoff (Hydrology)")) {
        ImGui::Text("Dataset: %zu samples", ctx.mlHydroDatasetSize);
        if (ImGui::Button("Collect Samples##Hydro")) callbacks_.mlCollectHydroData(ctx.mlSampleCount);
        ImGui::SameLine();
        if (ctx.mlHydroDatasetSize > 0) {
            if (ImGui::Button("Train Model##Hydro")) callbacks_.mlTrainHydroModel(ctx.mlTrainingEpochs, ctx.mlLearningRate);
        } else {
             ImGui::BeginDisabled();
             ImGui::Button("Train Model##Hydro");
             ImGui::EndDisabled();
        }
    }

    if (ImGui::CollapsingHeader("3. Fire Risk (Vegetation)")) {
        ImGui::Text("Dataset: %zu samples", ctx.mlFireDatasetSize);
        if (ImGui::Button("Collect Samples##Fire")) callbacks_.mlCollectFireData(ctx.mlSampleCount);
        ImGui::SameLine();
         if (ctx.mlFireDatasetSize > 0) {
            if (ImGui::Button("Train Model##Fire")) callbacks_.mlTrainFireModel(ctx.mlTrainingEpochs, ctx.mlLearningRate);
         } else {
             ImGui::BeginDisabled();
             ImGui::Button("Train Model##Fire");
             ImGui::EndDisabled();
         }
    }

    if (ImGui::CollapsingHeader("4. Biomass Growth (Vegetation)")) {
        ImGui::Text("Dataset: %zu samples", ctx.mlGrowthDatasetSize);
        if (ImGui::Button("Collect Samples##Growth")) callbacks_.mlCollectGrowthData(ctx.mlSampleCount);
        ImGui::SameLine();
         if (ctx.mlGrowthDatasetSize > 0) {
            if (ImGui::Button("Train Model##Growth")) callbacks_.mlTrainGrowthModel(ctx.mlTrainingEpochs, ctx.mlLearningRate);
         } else {
             ImGui::BeginDisabled();
             ImGui::Button("Train Model##Growth");
             ImGui::EndDisabled();
         }
    }
}

void UiLayer::drawVegetationInspector(UiFrameContext& ctx) {
    ImGui::Text("Grassland Ecosystem Model");
    
    const char* vegModes[] = { "OFF", "Realistic (Blend)", "Heatmap: EI (Grass)", "Heatmap: ES (Shrub)", "NDVI (Greenness)" };
    ImGui::Combo("Vis Mode", &ctx.vegetationMode, vegModes, IM_ARRAYSIZE(vegModes));
    
    if (ctx.vegetationMode > 0) {
        ImGui::Separator();
        ImGui::Text("Disturbance Regime:");
        ImGui::SliderFloat("Fire Prob", &ctx.disturbanceParams.fireFrequency, 0.0f, 0.1f, "%.4f");
        ImGui::SliderFloat("Grazing", &ctx.disturbanceParams.grazingIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Recovery Time", &ctx.disturbanceParams.averageRecoveryTime, 1.0f, 120.0f, "%.0f s");
        
        ImGui::Dummy(ImVec2(0,5));
        if (ImGui::Button("Ignite Fire")) callbacks_.triggerFireEvent();
        ImGui::SameLine();
        if (ImGui::Button("Reset Veg")) callbacks_.resetVegetation();
    }
}

void UiLayer::drawCrosshair(UiFrameContext& ctx) {
    if (ctx.camera.getCameraMode() != graphics::CameraMode::FreeFlight) return;

    // Draw Crosshair in center
    ImDrawList* drawList = ImGui::GetBackgroundDrawList(); // Background (behind windows) or Foreground (top)? Foreground.
    // Actually Foreground is better to ensure visibility.
    drawList = ImGui::GetForegroundDrawList();

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;

    float size = 10.0f;
    float thickness = 2.0f;
    ImU32 color = IM_COL32(255, 255, 255, 128); // Semi-transparent white
    ImU32 shadow = IM_COL32(0, 0, 0, 128);

    // Shadow
    drawList->AddLine(ImVec2(center.x - size, center.y + 1), ImVec2(center.x + size, center.y + 1), shadow, thickness);
    drawList->AddLine(ImVec2(center.x + 1, center.y - size), ImVec2(center.x + 1, center.y + size), shadow, thickness);

    // Main
    drawList->AddLine(ImVec2(center.x - size, center.y), ImVec2(center.x + size, center.y), color, thickness);
    drawList->AddLine(ImVec2(center.x, center.y - size), ImVec2(center.x, center.y + size), color, thickness);
    
    // Dot
    drawList->AddCircleFilled(center, 2.0f, IM_COL32(255,0,0,180));
}

} // namespace ui
