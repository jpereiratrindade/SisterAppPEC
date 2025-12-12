#include "ui_layer.h"

#include "../imgui_backend.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <array>
#include <iostream>
#include <utility>

namespace ui {

UiLayer::UiLayer(Callbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

void UiLayer::render(UiFrameContext& ctx, VkCommandBuffer cmd) {
    beginGuiFrame();
    drawDebugGui(0.0f, 0);

    drawStats(ctx);
    drawMenuBar(ctx);
    drawCamera(ctx);
    drawAnimation(ctx);
    drawBookmarks(ctx);
    drawResetCamera(ctx);

    endGuiFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UiLayer::drawStats(UiFrameContext& ctx) {
    if (!showStatsOverlay_) return;

    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 130), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Stats##Overlay", &showStatsOverlay_, flags)) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Mode: %s", ctx.camera.getCameraMode() == graphics::CameraMode::FreeFlight ? "Free Flight" : "Orbital");
        auto pos = ctx.camera.getPosition();
        ImGui::Text("Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        ImGui::Separator();
        ImGui::Text("Chunks: %d visible / %d total", ctx.visibleChunks, ctx.totalChunks);
        ImGui::Text("Pending Tasks: %d (veg: %d)", ctx.pendingTasks, ctx.pendingVeg);
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
                    constexpr std::array<const char*, 3> kTerrainModelOptions = {
                        "Plano com ondulações",
                        "Suave ondulado",
                        "Ondulado"
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
                    ImGui::Separator();
                    ImGui::Text("Resiliência do Terreno");
                    float resEcol = ctx.terrain->resilienceEcol();
                    if (ImGui::SliderFloat("Ecológica", &resEcol, 0.0f, 1.0f, "%.2f")) {
                        ctx.terrain->setResilienceEcol(resEcol);
                        if (ImGui::IsItemDeactivatedAfterEdit()) requestReset = true;
                    }
                    float resProd = ctx.terrain->resilienceProd();
                    if (ImGui::SliderFloat("Produtiva", &resProd, 0.0f, 1.0f, "%.2f")) {
                        ctx.terrain->setResilienceProd(resProd);
                        if (ImGui::IsItemDeactivatedAfterEdit()) requestReset = true;
                    }
                    float resSoc = ctx.terrain->resilienceSoc();
                    if (ImGui::SliderFloat("Social", &resSoc, 0.0f, 1.0f, "%.2f")) {
                        ctx.terrain->setResilienceSoc(resSoc);
                        if (ImGui::IsItemDeactivatedAfterEdit()) requestReset = true;
                    }
                    if (requestReset && callbacks_.requestTerrainReset) {
                        callbacks_.requestTerrainReset(1);
                    }
                    if (ImGui::Button("Regenerate Terrain")) {
                        if (callbacks_.requestTerrainReset) {
                            callbacks_.requestTerrainReset(1);
                        }
                    }
                }
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
                ImGui::TextWrapped("%s", ctx.lastSurfaceInfo.c_str());
            } else {
                ImGui::TextDisabled("Click (LMB) to probe surface type");
            }
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

} // namespace ui
