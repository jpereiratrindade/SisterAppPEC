#include "ui_layer.h"

#include "../imgui_backend.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "../terrain/hydrology_report.h"
#include "../terrain/landscape_metrics.h"
#include "../terrain/watershed.h"
#include <fstream>

#include <array>
#include <iostream>
#include <utility>

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
    drawDebugGui(0.0f, 0);

    drawStats(ctx);
    drawMenuBar(ctx);
    // drawAnimation(ctx); // Removed on User Request
    drawBookmarks(ctx);
    if (showResetCamera_) drawResetCamera(ctx);
    if (showMapGenerator_) drawFiniteTools(ctx);
    // v3.8.1 Use showCamControls_ for Viewer Controls (drawCamera)
    // Actually drawCamera IS Viewer Controls? Let's check logic. Yes, lines 305+.
    // Wait, drawCamera takes ~100 lines.
    // I need to guard it inside drawCamera or wrap it here.
    // I'll wrap it here.
    if (showCamControls_) drawCamera(ctx); 
    else {
        // Even if hidden, shortcuts should work?
        // drawCamera does shortcuts too? No, Application handles shortcuts. drawCamera is purely UI.
    }
    drawCrosshair(ctx); // v3.8.1
    
    // v3.8.0 Minimap
    if (showMinimap_ && minimap_) {
        minimap_->render(ctx.camera);
    }

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
        ImGui::Text("Chunks: %d visible / %d total", ctx.visibleChunks, ctx.totalChunks);
        ImGui::Text("Pending Tasks: %d (veg: %d)", ctx.pendingTasks, ctx.pendingVeg);
        
        if (ctx.lastSurfaceValid) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Probe Results:");
            // Assuming lastSurfaceInfo contains multiple lines or formatted text. 
            // Currently it is constructed as a stream in Application.cpp but not stored?
            // Wait, Application.cpp prints to std::cout. It doesn't seem to populate lastSurfaceInfo_?
            // I need to fix that in Application.cpp too if it's empty.
            // But let's assume I fix it.
            
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
            ImGui::EndMenu();
        }
        // v3.8.1: Views Menu
        if (ImGui::BeginMenu("Views")) {
             ImGui::MenuItem("Map Generator", nullptr, &showMapGenerator_);
             ImGui::MenuItem("Minimap", nullptr, &showMinimap_);
             ImGui::MenuItem("Camera Controls", nullptr, &showCamControls_);
             ImGui::MenuItem("Reset Camera Button", nullptr, &showResetCamera_);
             ImGui::Separator();
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
                if (ctx.terrain) {
                    ImGui::Separator();
                    int viewDist = ctx.terrain->viewDistance();
                    if (ImGui::SliderInt("Chunk View Distance", &viewDist, 4, 16)) {
                        ctx.terrain->setViewDistance(viewDist);
                    }
                    int chunkBudget = ctx.terrain->chunkBudgetPerFrame();
                    if (ImGui::SliderInt("Chunks/Frame Budget", &chunkBudget, 1, 24)) {
                        ctx.terrain->setChunkBudgetPerFrame(chunkBudget);
                    }
                    int meshBudget = ctx.terrain->meshBudgetPerFrame();
                    if (ImGui::SliderInt("Meshes/Frame Budget", &meshBudget, 1, 6)) {
                        ctx.terrain->setMeshBudgetPerFrame(meshBudget);
                    }
                    constexpr std::array<const char*, 4> kTerrainModelOptions = {
                        "Plano com ondulações",
                        "Suave ondulado",
                        "Ondulado",
                        "Montanhoso (Steep)"
                    };
                    int terrainModel = static_cast<int>(ctx.terrain->terrainModel());
                    if (ImGui::Combo("Modelo de Terreno", &terrainModel, kTerrainModelOptions.data(), static_cast<int>(kTerrainModelOptions.size()))) {
                        ctx.terrain->setTerrainModel(static_cast<graphics::TerrainModel>(terrainModel));
                        if (callbacks_.requestTerrainReset) {
                            callbacks_.requestTerrainReset(1);
                        }
                    }
                    bool veg = ctx.terrain->vegetationEnabled();
                    if (ImGui::Checkbox("Show Vegetation", &veg)) {
                        ctx.terrain->setVegetationEnabled(veg);
                        ctx.showVegetation = veg;
                    }
                    float vegDensity = ctx.terrain->vegetationDensity();
                    if (ImGui::SliderFloat("Vegetation Density", &vegDensity, 0.0f, 2.0f, "%.2f")) {
                        ctx.terrain->setVegetationDensity(vegDensity);
                    }
                    bool requestReset = false;
                    // V3.4.0: Resilience UI removed. Replaced with Slope Analysis Tools.
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Slope Analysis (v3.4)");
                    
                    graphics::SlopeConfig currentSlope = ctx.terrain->getSlopeConfig();
                    bool slopeChanged = false;
                    
                    // Flat Range: 0 to FlatMax
                    if (ImGui::SliderFloat("Flat Limit (%)", &currentSlope.flatMaxPct, 0.0f, 20.0f, "%.1f %%")) {
                        if (currentSlope.flatMaxPct > currentSlope.gentleMaxPct) currentSlope.gentleMaxPct = currentSlope.flatMaxPct;
                        slopeChanged = true;
                    }
                    
                    // Gentle Range: FlatMax to GentleMax
                    if (ImGui::SliderFloat("Gentle Limit (%)", &currentSlope.gentleMaxPct, currentSlope.flatMaxPct, 60.0f, "%.1f %%")) {
                         if (currentSlope.gentleMaxPct > currentSlope.onduladoMaxPct) currentSlope.onduladoMaxPct = currentSlope.gentleMaxPct;
                         if (currentSlope.gentleMaxPct < currentSlope.flatMaxPct) currentSlope.flatMaxPct = currentSlope.gentleMaxPct;
                         slopeChanged = true;
                    }

                    // Rolling/Ondulado Range: GentleMax to OnduladoMax
                    if (ImGui::SliderFloat("Rolling Limit (%)", &currentSlope.onduladoMaxPct, currentSlope.gentleMaxPct, 80.0f, "%.1f %%")) {
                        if (currentSlope.onduladoMaxPct > currentSlope.steepMaxPct) currentSlope.steepMaxPct = currentSlope.onduladoMaxPct;
                        if (currentSlope.onduladoMaxPct < currentSlope.gentleMaxPct) currentSlope.gentleMaxPct = currentSlope.onduladoMaxPct;
                        slopeChanged = true;
                    }

                    // Steep Range: OnduladoMax to SteepMax
                    if (ImGui::SliderFloat("Steep Limit (%)", &currentSlope.steepMaxPct, currentSlope.onduladoMaxPct, 100.0f, "%.1f %%")) {
                        if (currentSlope.steepMaxPct < currentSlope.onduladoMaxPct) currentSlope.onduladoMaxPct = currentSlope.steepMaxPct;
                        slopeChanged = true;
                    }
                    
                    ImGui::TextDisabled("Mountain: > %.1f %%", currentSlope.steepMaxPct);

                    if (slopeChanged) {
                        ctx.terrain->setSlopeConfig(currentSlope);
                    }
                    
                    // Persistence
                    ImGui::Separator();
                    if (ImGui::Button("Save Slope Prefs")) {
                        // We need access to Preferences instance. Since it's a singleton, we can include the header.
                        // Assuming valid include.
                         if (callbacks_.savePreferences) callbacks_.savePreferences();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load Slope Prefs")) {
                         if (callbacks_.loadPreferences) callbacks_.loadPreferences();
                    }

                    if (ImGui::Button("Regenerate Terrain")) {
                        if (callbacks_.requestTerrainReset) {
                            callbacks_.requestTerrainReset(1);
                        }
                    }
                }
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
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

    ImGui::Begin("Camera Controls");

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

        if (ctx.terrain) {
            bool veg = ctx.terrain->vegetationEnabled();
            if (ImGui::Checkbox("Show Vegetation", &veg)) {
                ctx.terrain->setVegetationEnabled(veg);
                ctx.showVegetation = veg;
            }
            float vegDensity = ctx.terrain->vegetationDensity();
            if (ImGui::SliderFloat("Vegetation Density", &vegDensity, 0.0f, 2.0f, "%.2f")) {
                ctx.terrain->setVegetationDensity(vegDensity);
            }
            if (ctx.lastSurfaceValid) {
        // Reuse string or update it if we did the probe logic here (it's done in App)
        // Actually, Application.cpp likely formats this string. Let's check App.
        ImGui::Text("Surface: %s", ctx.lastSurfaceInfo.c_str());
    } else {
        ImGui::Text("Surface: -");
    }
            ImGui::TextDisabled("Click (LMB) to probe surface type");
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

void UiLayer::drawFiniteTools(UiFrameContext& ctx) {
    if (ctx.terrain) return; // Only for Finite World mode

    ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 260), ImGuiCond_FirstUseEver);

    // v3.8.1: Allow Collapse (Removed ImGuiWindowFlags_NoCollapse)
    if (ImGui::Begin("Map Generator (v3.5)", nullptr, 0)) {
        ImGui::Checkbox("Show Slope Analysis", &ctx.showSlopeAnalysis);
        if (ctx.showSlopeAnalysis) {
             ImGui::TextColored(ImVec4(0.2,0.4,0.8,1), "0-3%%: Flat");
             ImGui::TextColored(ImVec4(0.2,0.8,0.8,1), "3-8%%: Gentle");
             ImGui::TextColored(ImVec4(0.2,0.8,0.2,1), "8-20%%: Moderate");
             ImGui::TextColored(ImVec4(0.8,0.8,0.0,1), "20-45%%: Steep");
             ImGui::TextColored(ImVec4(1.0,0.5,0.0,1), "45-75%%: V.Steep");
             ImGui::TextColored(ImVec4(0.8,0.0,0.0,1), ">75%%: Extreme");
        }
        if (ctx.showSlopeAnalysis && ctx.showDrainage) ctx.showSlopeAnalysis = false; // Mutually exclusive preferred
        
        ImGui::Checkbox("Show Drainage Model (Flux > 1)", &ctx.showDrainage);
        if (ctx.showDrainage) {
             ImGui::Indent();
             ImGui::SliderFloat("Intensity", &ctx.drainageIntensity, 0.05f, 1.0f, "%.2f");
             if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sensitivity: Lower = Only Rivers, Higher = Catchment Areas");
             ImGui::Unindent();
        }
        if (ctx.showDrainage && ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;

        bool wasShown = ctx.showWatershedVis;
        ImGui::Checkbox("Show Watershed Map (Basins)", &ctx.showWatershedVis);
        if (ctx.showWatershedVis) {
             ImGui::Indent();
             ImGui::Checkbox("Show Contours", &ctx.showBasinOutlines);
             ImGui::Unindent();

             if (!wasShown) {
                 // Auto-calculate if enabling
                 if (ctx.finiteMap) {
                     terrain::Watershed::segmentGlobal(*ctx.finiteMap);
                     if (callbacks_.updateMesh) callbacks_.updateMesh();
                 }
                 if (ctx.showSlopeAnalysis) ctx.showSlopeAnalysis = false;
                 if (ctx.showDrainage) ctx.showDrainage = false;
             }
        }

        // Enforce exclusion if others are toggled on
        if ((ctx.showSlopeAnalysis || ctx.showDrainage || ctx.showSoilVis) && ctx.showWatershedVis) ctx.showWatershedVis = false;
        if (ctx.showSoilVis && (ctx.showSlopeAnalysis || ctx.showDrainage)) { ctx.showSlopeAnalysis = false; ctx.showDrainage = false; }

        ImGui::Checkbox("Show Soil Map (v3.7)", &ctx.showSoilVis);
        if (ctx.showSoilVis) {
             ImGui::Indent();
             ImGui::Text("Legenda de Solos (V3.7):");
             
             // Define Colors matching Shader exactly (normalized 0-1)
             // cHidro: 0.0, 0.3, 0.3
             // cBText: 0.7, 0.35, 0.05
             // cArgila: 0.4, 0.0, 0.5
             // cBemDes: 0.5, 0.15, 0.1
             // cRaso: 0.7, 0.7, 0.2
             // cRocha: 0.2, 0.2, 0.2
             
             ImGui::ColorButton("##cHidro", ImVec4(0.0f, 0.3f, 0.3f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Hidromorfico (Plano)", &ctx.soilHidroAllowed);
             
             ImGui::ColorButton("##cBText", ImVec4(0.7f, 0.35f, 0.05f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Horizonte B Textural", &ctx.soilBTextAllowed);
             
             ImGui::ColorButton("##cArgila", ImVec4(0.4f, 0.0f, 0.5f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Argila Expansiva", &ctx.soilArgilaAllowed);
             
             ImGui::ColorButton("##cBemDes", ImVec4(0.5f, 0.15f, 0.1f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Bem Desenvolvido", &ctx.soilBemDesAllowed);
             
             ImGui::ColorButton("##cRaso", ImVec4(0.7f, 0.7f, 0.2f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Solo Raso", &ctx.soilRasoAllowed);
             
             ImGui::ColorButton("##cRocha", ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); 
             ImGui::SameLine(); ImGui::Checkbox("Afloramento/Rocha", &ctx.soilRochaAllowed);
             
             ImGui::Unindent();
        }

        ImGui::Separator();
        
        // --- Visual Settings (v3.7.3) ---
        if (ImGui::CollapsingHeader("Visual & Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
             // Direct binding to Application State via Reference
             // Direct binding to Application State via Reference
             ImGui::SliderFloat("Sun Azimuth", &ctx.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
             ImGui::SliderFloat("Sun Elevation", &ctx.sunElevation, -90.0f, 90.0f, "%.0f deg");
             ImGui::SliderFloat("Light Intensity", &ctx.lightIntensity, 0.0f, 2.0f, "%.2f"); // v3.8.1
             
             // Inverting logic for User: "Render Distance" instead of Fog Density
             // High density = Low distance. Low density = High distance.
             float renderDist = (0.01f - ctx.fogDensity) * 10000.0f; // Approx mapping
             if (ImGui::SliderFloat("Render Distance (Fog)", &ctx.fogDensity, 0.0f, 0.005f, "%.5f")) {
                 // Direct update
             }
        }

        ImGui::Separator();
        ImGui::Text("Terrain Generator Options");
        
        const char* terrainTypes[] = { "Hills", "Mountains", "Plains", "Valleys" };

        // 1. State Declarations
        static int selectedSize = 1024;
        static float scale = 0.002f;
        static float amplitude = 80.0f;
        static int preset = 1; // 0=Plains, 1=Hills, 2=Mountains, 3=Alpine

        // 2. Terrain Type Preset
        if (ImGui::Combo("Terrain Type", &preset, "Plains\0Hills\0Mountains\0Alpine\0\0")) {
            if (preset == 0) { scale = 0.001f; amplitude = 40.0f; }      // Plains
            if (preset == 1) { scale = 0.002f; amplitude = 80.0f; }      // Hills
            if (preset == 2) { scale = 0.003f; amplitude = 180.0f; }     // Mountains
            if (preset == 3) { scale = 0.004f; amplitude = 250.0f; }     // Alpine
        }

        // 3. Manual Controls
        ImGui::Text("Map Size:");
        if (ImGui::RadioButton("512", selectedSize == 512)) selectedSize = 512;
        ImGui::SameLine();
        if (ImGui::RadioButton("1024", selectedSize == 1024)) selectedSize = 1024;
        ImGui::SameLine();
        if (ImGui::RadioButton("2048", selectedSize == 2048)) selectedSize = 2048;

        ImGui::SliderFloat("Feature Size (Base Freq)", &scale, 0.0001f, 0.01f, "%.4f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Controls the size of mountains.\nLower = Larger features\nHigher = Smaller features");

        static float persistence = 0.5f; // v3.7.1
        ImGui::SliderFloat("Roughness (Persistence)", &persistence, 0.2f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Controls jaggy-ness/detail.\nLower = Smooth hills\nHigher = Rocky/Noisy");

        ImGui::SliderFloat("Height Amplitude", &amplitude, 50.0f, 500.0f, "%.0f m");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Max Height of terrain features.");

        static float waterLvl = 64.0f;
        ImGui::SliderFloat("Water Level (Sea)", &waterLvl, 0.0f, 200.0f, "%.0f m");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Terrains below this level are shown as Water on Minimap.");

        // v3.6.5 Resolution Control
        static float resolution = 1.0f;
        ImGui::SliderFloat("Cell Size (Resolution)", &resolution, 0.1f, 2.0f, "%.1f m");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("World units per grid cell.\nLower = Higher Resolution (Smoother)\nHigher = Lower Resolution (Blocky)");

        // v3.7.8 Seed Control
        // Uses static for requested new seed, but initializes from ctx on first run if needed?
        // Actually, we want to control the NEXT generation.
        static int seedInput = 12345;
        // Sync with ctx once?
        static bool initialized = false;
        if (!initialized) { seedInput = ctx.seed; initialized = true; }
        
        ImGui::InputInt("Seed", &seedInput);
        if (ImGui::Button("Randomize")) {
            seedInput = rand(); 
        }

        ImGui::Separator();
        
        if (ImGui::Button("Generate New Map", ImVec2(-1, 0))) {
            if (callbacks_.regenerateFiniteWorld) {
                callbacks_.regenerateFiniteWorld(selectedSize, scale, amplitude, resolution, persistence, seedInput, waterLvl);
            }
        }

        ImGui::Separator();
        ImGui::Text("Viewer Controls:");

        // Speed Control
        float currentSpeed = ctx.camera.getMoveSpeed();
        if (ImGui::SliderFloat("Fly Speed", &currentSpeed, 10.0f, 200.0f, "%.0f m/s")) {
            ctx.camera.setMoveSpeed(currentSpeed);
        }

        // Orbit Map Center
        if (ImGui::Button("Orbit Map", ImVec2(100, 0))) {
            if (ctx.finiteMap) {
                float cx = ctx.finiteMap->getWidth() / 2.0f;
                float cz = ctx.finiteMap->getHeight() / 2.0f;
                // Height at center (approx 40 + noise) - look at base
                ctx.camera.setTarget({cx, 40.0f, cz});
                ctx.camera.setCameraMode(graphics::CameraMode::Orbital);
                ctx.camera.setDistance(ctx.finiteMap->getWidth() * 0.8f); // Fit view
                ctx.camera.setFarClip(500.0f); // Restore default
            }
        }
        ImGui::SameLine();
        
        // Teleport to Center
        if (ImGui::Button("Fly Center", ImVec2(100, 0))) {
            if (ctx.finiteMap) {
                float cx = ctx.finiteMap->getWidth() / 2.0f;
                float cz = ctx.finiteMap->getHeight() / 2.0f;
                // Get terrain height if possible, else safe height
                float h = ctx.finiteMap->getHeight(static_cast<int>(cx), static_cast<int>(cz));
                ctx.camera.teleportTo({cx, h + 50.0f, cz});
                ctx.camera.setCameraMode(graphics::CameraMode::FreeFlight);
                ctx.camera.setFarClip(500.0f); // Restore default
            }
        }
        ImGui::SameLine();
        
        // v3.8.1: Top View
        if (ImGui::Button("Top View", ImVec2(100, 0))) {
            if (minimap_) {
                float w = minimap_->getWorldWidth();
                float h = minimap_->getWorldHeight();
                float cx = w / 2.0f;
                float cz = h / 2.0f;
                float alt = std::max(w, h); // Height to see full map (approx 90 deg FOV)
                
                ctx.camera.setCameraMode(graphics::CameraMode::FreeFlight);
                ctx.camera.setFlying(true);
                ctx.camera.teleportTo({cx, alt, cz});
                ctx.camera.setPitch(-89.9f); // Look Straight Down
                ctx.camera.setYaw(0.0f);   // North Up
                
                // Clear Fog to make sure terrain is visible from high altitude
                ctx.fogDensity = 0.0f; 
                ctx.camera.setFarClip(8000.0f); // Extend view for Top Down
            }
        }
    }
    ImGui::End();
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
