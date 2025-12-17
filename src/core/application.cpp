#include "application.h"
#include "../graphics/geometry_utils.h"
#include "../terrain/watershed.h" // v3.6.3
#include "../imgui_backend.h"
#include "../math/math_types.h"
#include "preferences.h" // v3.4.0
#include "version.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdio> // for snprintf
#include <cstring> // for std::memcpy
#include <iomanip> // v3.7.0
#include <iterator>

namespace core {

// Helper for Raycasting Finite Terrain
bool raycastFiniteTerrain(const terrain::TerrainMap& map, const math::Ray& ray, float maxDist, float gridScale, int& outX, int& outZ, math::Vec3& outHitPos) {
    float t = 0.0f;
    float step = 0.5f * gridScale; // Precision scales with grid
    
    while (t < maxDist) {
        math::Vec3 p = ray.origin + ray.direction * t;
        
        int x = static_cast<int>(std::round(p.x / gridScale));
        int z = static_cast<int>(std::round(p.z / gridScale));
        
        if (x >= 0 && x < map.getWidth() && z >= 0 && z < map.getHeight()) {
            float h = map.getHeight(x, z);
            if (p.y <= h) {
                outX = x;
                outZ = z;
                outHitPos = p; // Capture exact hit position
                return true;
            }
        }
        t += step;
    }
    return false;
}

Application::Application() 
    : camera_(60.0f * 3.14159f / 180.0f, 16.0f/9.0f, 0.1f, 500.0f) 
{
    init();
}

Application::~Application() {
    cleanup();
}

void Application::init() {
    std::string title = std::string(APP_NAME) + " - " + std::string(APP_VERSION_TAG);
    if (!initSDL(sdl_, title.c_str(), 1280, 720)) {
        throw std::runtime_error("Failed to initialize SDL");
    }

    ctx_ = std::make_unique<GraphicsContext>(sdl_.window, true); // Validation enabled by default for debugging

    swapchain_ = std::make_unique<Swapchain>(*ctx_, sdl_.window, vsyncEnabled_);
    commandPool_ = std::make_unique<CommandPool>(*ctx_, ctx_->queueFamilyIndex());
    syncObjects_ = std::make_unique<SyncObjects>(*ctx_, static_cast<uint32_t>(swapchain_->images().size()));

    imagesInFlight_.resize(swapchain_->images().size(), VK_NULL_HANDLE);

    commandBuffers_ = commandPool_->allocate(static_cast<uint32_t>(swapchain_->images().size()));

    if (!initImGuiCore(sdl_, *ctx_, descriptorPool_)) throw std::runtime_error("ImGui Core Init Failed");
    if (!initImGuiVulkan(*ctx_, swapchain_->renderPass(), static_cast<uint32_t>(swapchain_->images().size()), descriptorPool_)) throw std::runtime_error("ImGui Vulkan Init Failed");
    
    if (!renderer_.init()) throw std::runtime_error("Renderer Init Failed");

    wireframeSupported_ = ctx_->supportsWireframe();
    
    lineMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_FILL);
    pointMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_POLYGON_MODE_FILL);
    solidMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
    
    if (wireframeSupported_) {
        wireframeMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE);
    }
    
    // Environment material with enhanced shaders
    {
        auto vs = std::make_shared<graphics::Shader>(*ctx_, "shaders/environment.vert.spv");
        auto fs = std::make_shared<graphics::Shader>(*ctx_, "shaders/environment.frag.spv");
        environmentMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), vs, fs, 
                                                                     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
    }
    
    // Voxel Material Removed

    std::vector<graphics::Vertex> gridVerts;
    std::vector<uint16_t> gridIndices;
    graphics::createGrid(gridVerts, gridIndices, 50); // Expanded to 50x50
    gridMesh_ = std::make_unique<graphics::Mesh>(*ctx_, gridVerts, gridIndices);

    std::vector<graphics::Vertex> axesVerts;
    std::vector<uint16_t> axesIndices;
    graphics::createAxes(axesVerts, axesIndices);
    axesMesh_ = std::make_unique<graphics::Mesh>(*ctx_, axesVerts, axesIndices);
    
    // NEW: Sky Dome
    std::vector<graphics::Vertex> skyVerts;
    std::vector<uint16_t> skyIndices;
    graphics::createSkyDome(skyVerts, skyIndices, 5000.0f, 32); // Huge dome to cover world (Must be < zFar)
    skyDomeMesh_ = std::make_unique<graphics::Mesh>(*ctx_, skyVerts, skyIndices);
    
    // NEW: Distance Markers
    std::vector<graphics::Vertex> markersVerts;
    std::vector<uint16_t> markersIndices;
    graphics::createDistanceMarkers(markersVerts, markersIndices, 50, 10);
    distanceMarkersMesh_ = std::make_unique<graphics::Mesh>(*ctx_, markersVerts, markersIndices);
    
    // --- V3.5.0: Finite World Initialization ---
    // Defaulting to Finite World
    std::cout << "[SisterApp v3.5.0] Initializing Finite World (1024x1024)..." << std::endl;
    finiteMap_ = std::make_unique<terrain::TerrainMap>(1024, 1024);
    // v3.7.8: Use Config Seed
    finiteGenerator_ = std::make_unique<terrain::TerrainGenerator>(currentSeed_);
    
    // Pass the correct RenderPass!
    finiteRenderer_ = std::make_unique<shape::TerrainRenderer>(*ctx_, swapchain_->renderPass());
    
    terrain::TerrainConfig config;
    config.maxHeight = 80.0f; // Gentle rolling hills
    config.seed = currentSeed_; // v3.7.8
    
    // Match default deferred config
    deferredConfig_.width = 1024;
    deferredConfig_.height = 1024;
    deferredConfig_.maxHeight = 80.0f;
    deferredConfig_.seed = currentSeed_;
    finiteGenerator_->generateBaseTerrain(*finiteMap_, config); // Will re-seed
    
    // Re-enable erosion for Drainage Visualization
    finiteGenerator_->applyErosion(*finiteMap_, 250000);
    
    // v3.6.3: Ensure Drainage is calculated on startup
    finiteGenerator_->calculateDrainage(*finiteMap_);
    
    // v3.7.3: Semantic Soil Classification
    finiteGenerator_->classifySoil(*finiteMap_, config);
    
    // Update mesh for 3D View
    finiteRenderer_->buildMesh(*finiteMap_);

    camera_.setCameraMode(graphics::CameraMode::FreeFlight);
    float cx = 1024.0f / 2.0f;
    float cz = 1024.0f / 2.0f;
    float h = finiteMap_->getHeight(static_cast<int>(cx), static_cast<int>(cz));
    camera_.teleportTo({cx, h + 60.0f, cz}); // Higher initial flight
    camera_.setPitch(-20.0f); // Look down to see more terrain (Horizon higher)
    camera_.setFovDegrees(60.0f); // Wider view

    
    std::cout << "[SisterApp v3.5.0] Finite World Ready!" << std::endl;

    // V3.4.0: Load preferences on startup
    // v3.8.1: Preferences Disabled
    // core::Preferences::instance().load();
    // core::Preferences::instance().load();
    // if (finiteMap_) { ... }

    ui::Callbacks uiCallbacks{
        [this](const std::string& name) { saveBookmark(name); },
        [this](size_t index) { loadBookmark(index); },
        [this](size_t index) { deleteBookmark(index); },
        [this](int warmup) { requestTerrainReset(warmup); },
        [this]() { // savePreferences
            // v3.8.1 Disabled
            // if (terrain_) {
            //     core::Preferences::instance().setSlopeConfig(terrain_->getSlopeConfig());
            // }
            // core::Preferences::instance().save();
        },
        [this]() { // loadPreferences
            // v3.8.1 Disabled
            // core::Preferences::instance().load();
            // if (terrain_) {
            //     terrain_->setSlopeConfig(core::Preferences::instance().getSlopeConfig());
            // }
        },
        // v3.6.5 Resolution
        // v3.6.5 Resolution + v3.7.8 Seed + v3.8.0 Water Level
        [this](const terrain::TerrainConfig& config) { // regenerateFiniteWorld
            this->regenerateFiniteWorld(config);
        },
        [this]() { // updateMesh
            // Defer to next frame start
            meshUpdateRequested_ = true;
        }
    };
    // UI Layer
    // UI Layer
    uiLayer_ = std::make_unique<ui::UiLayer>(*ctx_, uiCallbacks);
    uiLayer_->applyTheme(ui::Theme::Dark);

    camera_.setAspect(static_cast<float>(swapchain_->extent().width) / static_cast<float>(swapchain_->extent().height));
}

void Application::cleanup() {
    // Shutdown in reverse order
    if (uiLayer_) {
        uiLayer_.reset(); // Destroy UI and Minimap BEFORE shutting down Vulkan context
    }

    // Wait for idle to ensure no commands are pending before destroying ANY resources
    if (ctx_) {
        vkDeviceWaitIdle(ctx_->device());
    }

    // Terrain contains Vulkan buffers that must be destroyed while device is valid
    // terrain_ removed
    finiteRenderer_.reset();
    finiteGenerator_.reset();
    finiteMap_.reset();
    
    // v3.3.0: Explicitly destroy sync objects - DO NOT RESET TWICE
    syncObjects_.reset(); // Fences/Semaphores need device
    
    shutdownImGuiVulkanIfNeeded();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    renderer_.destroy();

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->device(), descriptorPool_, nullptr);
    }
    
    distanceMarkersMesh_.reset(); // NEW
    skyDomeMesh_.reset();          // NEW
    gridMesh_.reset();
    axesMesh_.reset();
    
    // Materials must be destroyed before device (includes pipelines)
    // voxelMaterial_ removed
    // waterMaterial_ removed
    environmentMaterial_.reset();  // NEW: Environment shaders
    wireframeMaterial_.reset();
    solidMaterial_.reset();
    pointMaterial_.reset();
    lineMaterial_.reset();
    
    // syncObjects_.reset(); // REMOVED DUPLICATE RESET
    
    // Free command buffers
    // Free command buffers
    if (commandPool_ && !commandBuffers_.empty()) {
        commandPool_->free(commandBuffers_);
        commandBuffers_.clear();
    }
    
    // Clear in-flight fence references
    imagesInFlight_.clear();

    commandPool_.reset();
    swapchain_.reset();
    ctx_.reset(); 
    
    destroySDL(sdl_);
}

void Application::run() {
    std::uint64_t prevCounter = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    inputManager_.setLastInputSeconds(static_cast<double>(SDL_GetTicks()) / 1000.0);

    // Idle limit logic
    while (running_) {
        // V3.5.0: Check for deferred regeneration tasks BEFORE frame start
        performRegeneration();

        std::uint64_t frameStart = SDL_GetPerformanceCounter();
        double deltaSeconds = static_cast<double>(frameStart - prevCounter) / freq;
        prevCounter = frameStart;

        double currentTime = static_cast<double>(SDL_GetTicks()) / 1000.0;
        
        // Idle Check: If no input for 5 seconds and Window is not minimized
        bool isIdle = (limitIdleFps_ && (currentTime - inputManager_.lastInputSeconds() > 5.0));

        processEvents(deltaSeconds);
        update(deltaSeconds);
        
        if (regenRequested_) {
            performRegeneration();
        }
        if (meshUpdateRequested_) {
            performMeshUpdate();
        }

        if (running_) {
            render(currentFrame_);
            
            // Sleep if idle to save power (~20 FPS)
            if (isIdle) {
                SDL_Delay(50); 
            } else {
                // Choose cap: if VSync off, enforce a higher cap to avoid runaway frames
                float capFps = fpsCapEnabled_ ? fpsCapTarget_ : 0.0f;
                if (!vsyncEnabled_ && vsyncOffFpsCap_ > 1.0f) {
                    capFps = (capFps > 0.0f) ? std::min(capFps, vsyncOffFpsCap_) : vsyncOffFpsCap_;
                }
                if (capFps > 1.0f) {
                    double targetMs = 1000.0 / static_cast<double>(capFps);
                    std::uint64_t frameEnd = SDL_GetPerformanceCounter();
                    double elapsedMs = (static_cast<double>(frameEnd - frameStart) / freq) * 1000.0;
                    if (targetMs > elapsedMs) {
                        double remainingMs = targetMs - elapsedMs;
                        SDL_Delay(static_cast<Uint32>(remainingMs));
                    }
                }
            }
        }
        
        if (!needsRecreate_) {
            currentFrame_ = (currentFrame_ + 1) % swapchain_->images().size();
        }
    }
}

void Application::processEvents(double dt) {
    (void)dt; // Unused for now
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        inputManager_.recordEvent(event);
        double currentTimeSeconds = static_cast<double>(SDL_GetTicks()) / 1000.0;
        
        bool imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;
        bool isMouseEvent = (event.type == SDL_MOUSEMOTION || 
                             event.type == SDL_MOUSEBUTTONDOWN || 
                             event.type == SDL_MOUSEBUTTONUP || 
                             event.type == SDL_MOUSEWHEEL);
        
        if (!imguiWantsMouse || !isMouseEvent) {
            camera_.processEvent(event);
            
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (finiteMap_) { // v3.7.0 Probe
                     float w = static_cast<float>(swapchain_->extent().width);
                     float h = static_cast<float>(swapchain_->extent().height);
// ... Rest of Finite Probe Logic ...
                     math::Vec3 rayDir = camera_.getRayDirection(static_cast<float>(event.button.x), static_cast<float>(event.button.y), w, h);
                     math::Ray ray{camera_.getPosition(), rayDir};
                     
                     int hitX, hitZ;
                     math::Vec3 hitPos;
                     if (raycastFiniteTerrain(*finiteMap_, ray, 1000.0f * worldResolution_, worldResolution_, hitX, hitZ, hitPos)) {
                         // Calculate Slope
                         float hL = finiteMap_->getHeight(std::max(0, hitX-1), hitZ);
                         float hR = finiteMap_->getHeight(std::min(finiteMap_->getWidth()-1, hitX+1), hitZ);
                         float hD = finiteMap_->getHeight(hitX, std::max(0, hitZ-1));
                         float hU = finiteMap_->getHeight(hitX, std::min(finiteMap_->getHeight()-1, hitZ+1));
                         
                         float dz_dx = (hR - hL) / (2.0f * worldResolution_);
                         float dz_dz = (hU - hD) / (2.0f * worldResolution_);
                         
                         float slopePct = std::sqrt(dz_dx*dz_dx + dz_dz*dz_dz) * 100.0f;
                         
                         // v3.7.3: Semantic Probe (CPU Authority)
                         auto soilId = finiteMap_->getSoil(hitX, hitZ);
                         std::string soilType;
                         std::memset(lastSurfaceColor_, 0, 12);
                         
                         // Match SoilType enum
                         // Hidro=1, BText=2, Argila=3, BemDes=4, Raso=5, Rocha=6
                         
                         // Helper for Color Copy
                         auto setColor = [&](float r, float g, float b) {
                             float c[3] = {r, g, b};
                             std::memcpy(lastSurfaceColor_, c, 12);
                         };

                         switch (soilId) {
                             case terrain::SoilType::Hidromorfico:
                                 soilType = "Hidromorfico";
                                 setColor(0.0f, 0.3f, 0.3f);
                                 break;
                             case terrain::SoilType::BTextural:
                                 soilType = "Horizonte B textural";
                                 setColor(0.7f, 0.35f, 0.05f);
                                 break;
                             case terrain::SoilType::Argila:
                                 soilType = "Presenca de argila expansiva";
                                 setColor(0.4f, 0.0f, 0.5f);
                                 break;
                             case terrain::SoilType::BemDes:
                                 soilType = "Solo bem desenvolvido";
                                 setColor(0.5f, 0.15f, 0.1f);
                                 break;
                             case terrain::SoilType::Raso:
                                 soilType = "Solo Raso";
                                 setColor(0.7f, 0.7f, 0.2f);
                                 break;
                             case terrain::SoilType::Rocha:
                                 soilType = "Afloramento rochoso";
                                 setColor(0.2f, 0.2f, 0.2f);
                                 break;
                             default:
                                 soilType = "Indefinido";
                                 setColor(0.1f, 0.1f, 0.1f);
                                 break;
                         }
                         
                         // v3.7.1: Expanded Probe Data
                         float elevation = finiteMap_->getHeight(hitX, hitZ);
                         float flux = finiteMap_->fluxMap()[hitZ * finiteMap_->getWidth() + hitX];
                         int basinID = finiteMap_->watershedMap()[hitZ * finiteMap_->getWidth() + hitX];
                         
                         std::stringstream ss;
                         ss << "Loc: (" << hitX << ", " << hitZ << ")\n";
                         ss << "Elev: " << std::fixed << std::setprecision(2) << elevation << " m\n";
                         ss << "Decliv: " << std::setprecision(1) << slopePct << "%\n";
                         ss << "Solo: " << soilType << "\n";
                         ss << "Fluxo: " << std::setprecision(1) << flux << " m2\n";
                         ss << "Bacia ID: " << basinID;
                         
                         lastSurfaceInfo_ = ss.str();
                         lastSurfaceValid_ = true;
                         
                         // Keep console output for debug
                         std::cout << "[Probe] " << lastSurfaceInfo_ << std::endl;
                         std::cout << "-----------------------------------" << std::endl;
                     }
                }
            }

            // Interactive Delineation (Right Click)
            // v3.8.1 Disabled by User Request (to avoid conflict with Camera Look)
            /*
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                if (useFiniteWorld_ && finiteMap_) {
                     float w = static_cast<float>(swapchain_->extent().width);
                     float h = static_cast<float>(swapchain_->extent().height);
                     math::Vec3 rayDir = camera_.getRayDirection(static_cast<float>(event.button.x), static_cast<float>(event.button.y), w, h);
                     math::Ray ray{camera_.getPosition(), rayDir};
                     
                     int hitX, hitZ;
                     math::Vec3 hitPos;
                     if (raycastFiniteTerrain(*finiteMap_, ray, 1000.0f * worldResolution_, worldResolution_, hitX, hitZ, hitPos)) {
                         std::cout << "[Delineation] Hit at (" << hitX << ", " << hitZ << ")" << std::endl;
                         
                         // Clear previous
                         std::fill(finiteMap_->watershedMap().begin(), finiteMap_->watershedMap().end(), 0);
                         
                         // Delineate
                         terrain::Watershed::delineate(*finiteMap_, hitX, hitZ, 1);
                         
                         // Show result
                         showWatershedVis_ = true;
                         showSlopeAnalysis_ = false; 
                         showDrainage_ = false;

                         // Trigger upload (Mesh update needed? No, texture update usually, but here vertex attr update. 
                         // Vertex update is required because BasinID is in vertex attributes now)
                         meshUpdateRequested_ = true;
                     }
                }
            }
            */


        }
        
        if (event.type == SDL_QUIT) {
            running_ = false;
        }
        if (event.type == SDL_WINDOWEVENT && 
           (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
            needsRecreate_ = true;
        }
        
        // Keyboard Events (Single Press / Repeat)
        if (event.type == SDL_KEYDOWN) {
            // Camera mode toggle (Tab or C key) - Single Trigger
            if ((event.key.keysym.sym == SDLK_TAB || event.key.keysym.sym == SDLK_c) && event.key.repeat == 0) {
                if (camera_.getCameraMode() == graphics::CameraMode::Orbital) {
                    camera_.setCameraMode(graphics::CameraMode::FreeFlight);
                    std::cout << "[Camera] Switched to Free Flight Mode (WASD + Right Mouse)" << std::endl;
                    inputManager_.setLastInputSeconds(currentTimeSeconds);
                } else {
                    camera_.setCameraMode(graphics::CameraMode::Orbital);
                    std::cout << "[Camera] Switched to Orbital Mode (Mouse Drag)" << std::endl;
                    inputManager_.setLastInputSeconds(currentTimeSeconds);
                }
            }

            // FOV quick adjust (Allow Repeat)
            if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
                camera_.setFovDegrees(camera_.getFovDegrees() - 5.0f);
            }
            if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                camera_.setFovDegrees(camera_.getFovDegrees() + 5.0f);
            }
            
            // Single Trigger Actions
            if (event.key.repeat == 0) {
                // Reset camera to origin (R key)
                if (event.key.keysym.sym == SDLK_r) {
                    camera_.teleportTo({0.0f, 60.0f, 0.0f}); // High above terrain
                    std::cout << "[Camera] Teleported to Origin (High)" << std::endl;
                }
                
                // Quick teleport shortcuts (1-4 keys)
                if (event.key.keysym.sym == SDLK_1) {
                    camera_.teleportTo({10.0f, 60.0f, 10.0f});
                    std::cout << "[Camera] Teleported to Quadrant +X+Z" << std::endl;
                }
                if (event.key.keysym.sym == SDLK_2) {
                    camera_.teleportTo({-10.0f, 60.0f, 10.0f});
                    std::cout << "[Camera] Teleported to Quadrant -X+Z" << std::endl;
                }
                if (event.key.keysym.sym == SDLK_3) {
                    camera_.teleportTo({-10.0f, 60.0f, -10.0f});
                    std::cout << "[Camera] Teleported to Quadrant -X-Z" << std::endl;
                }
                if (event.key.keysym.sym == SDLK_4) {
                    camera_.teleportTo({10.0f, 60.0f, -10.0f});
                    std::cout << "[Camera] Teleported to Quadrant +X-Z" << std::endl;
                }
                
                // Bookmark shortcuts (F5-F8)
                if (event.key.keysym.sym == SDLK_F5) {
                    saveBookmark("Quick Slot 1");
                }
                if (event.key.keysym.sym == SDLK_F6 && !bookmarks_.empty()) {
                    loadBookmark(0);
                }
                if (event.key.keysym.sym == SDLK_F7 && bookmarks_.size() > 1) {
                    loadBookmark(1);
                }
                if (event.key.keysym.sym == SDLK_F8 && bookmarks_.size() > 2) {
                    loadBookmark(2);
                }
                
                // Jump (Space key)
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (camera_.getCameraMode() == graphics::CameraMode::FreeFlight) {
                        camera_.jump();
                    }
                }
            }
        }
    } // End While loop

    // Check held keys for idle prevention
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    inputManager_.recordKeyboardState(state);
}

void Application::rebuildMaterials() {
    lineMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_FILL);
    pointMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_POLYGON_MODE_FILL);
    solidMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
    
    if (wireframeSupported_) {
        wireframeMaterial_ = createMaterial(swapchain_->renderPass(), swapchain_->extent(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE);
    } else {
        wireframeMaterial_.reset(); // Ensure it's null if not supported
    }
    auto envVs = std::make_shared<graphics::Shader>(*ctx_, "shaders/environment.vert.spv");
    auto envFs = std::make_shared<graphics::Shader>(*ctx_, "shaders/environment.frag.spv");
    environmentMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), envVs, envFs,
                                                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
}
void Application::update(double dt) {
    Uint32 nowMs = SDL_GetTicks();
    // Process keyboard input for camera movement
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    camera_.processKeyboard(keyState, static_cast<float>(dt));

    camera_.update(static_cast<float>(dt));
    
    // Update animations
    if (animationEnabled_) {
        gridAnimator_.update(static_cast<float>(dt));
        axesAnimator_.update(static_cast<float>(dt));
    }

}

void Application::recreateSwapchain() {
    vkDeviceWaitIdle(ctx_->device());
    
    shutdownImGuiVulkanIfNeeded();
    renderer_.destroy();
    
    if (commandPool_) {
        commandPool_->free(commandBuffers_);
    }
    commandBuffers_.clear(); 

    syncObjects_.reset();

    swapchain_->recreate(sdl_.window, vsyncEnabled_);
    syncObjects_ = std::make_unique<SyncObjects>(*ctx_, static_cast<uint32_t>(swapchain_->images().size()));

    // Reset imagesInFlight to prevent using stale fences
    imagesInFlight_.clear();
    imagesInFlight_.resize(swapchain_->images().size(), VK_NULL_HANDLE);

    commandBuffers_ = commandPool_->allocate(static_cast<uint32_t>(swapchain_->images().size()));
    
    if (!initImGuiVulkan(*ctx_, swapchain_->renderPass(), static_cast<uint32_t>(swapchain_->images().size()), descriptorPool_)) {
         throw std::runtime_error("ImGui Vulkan Re-Init Failed");
    }
    
    renderer_.init();
    
    rebuildMaterials();
    
    camera_.setAspect(static_cast<float>(swapchain_->extent().width) / static_cast<float>(swapchain_->extent().height));
    
    currentFrame_ = 0;
    needsRecreate_ = false;
}

void Application::render(size_t frameIndex) {
    if (needsRecreate_) {
        recreateSwapchain();
        return;
    }

    syncObjects_->waitForFence(frameIndex);

    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(ctx_->device(), swapchain_->handle(), UINT64_MAX,
                                         syncObjects_->imageAvailable(frameIndex), VK_NULL_HANDLE, &imageIndex);
    
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        needsRecreate_ = true;
        return;
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(ctx_->device(), 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    imagesInFlight_[imageIndex] = syncObjects_->inFlight(frameIndex);

    syncObjects_->resetFence(frameIndex);
    vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
    VkCommandBuffer cmd = commandBuffers_[imageIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.529f, 0.808f, 0.922f, 1.0f} }; // Sky Blue (SkyBlue) 
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = swapchain_->renderPass();
    rpBegin.framebuffer = swapchain_->framebuffers()[imageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = swapchain_->extent();
    rpBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // --- Scene Rendering ---
    const float* view = camera_.viewMatrix();
    const float* proj = camera_.projectionMatrix();
    
    float mvp[16];
    auto mulMat4 = [](const float a[16], const float b[16], float out[16]) {
        for(int r=0; r<4; ++r) {
            for(int c=0; c<4; ++c) {
                out[c*4+r] = 0.0f;
                for(int k=0; k<4; ++k) {
                    out[c*4+r] += a[k*4+r] * b[c*4+k];
                }
            }
        }
    };
    mulMat4(proj, view, mvp); 

    // Sky Dome - Re-enabled for V3.5.0
    // v3.8.1 Fix: Sky Dome logic must center on camera to appear infinite
    if (skyDomeMesh_) {
        // Create Sky View Matrix (Remove Translation)
        float skyView[16];
        std::copy(view, view+16, skyView);
        // Column-major translation is indices 12, 13, 14
        skyView[12] = 0.0f;
        skyView[13] = 0.0f;
        skyView[14] = 0.0f;
        
        float skyMVP[16];
        mulMat4(proj, skyView, skyMVP);

        graphics::RenderOptions opts;
        opts.pointSize = 1.0f;
        opts.useLighting = false; // Sky is self-illuminated (unlit)
        // Disable Depth Write so Sky is always background
        // Wait, renderer record doesn't expose depth write toggle easily here unless Material handles it?
        // environmentMaterial should probably have depthWrite=false.
        // For now, centering it prevents the "Sphere on horizon" issue significantly.
        renderer_.record(cmd, skyDomeMesh_.get(), environmentMaterial_.get(), swapchain_->extent(), skyMVP, opts);
    }

    // V3.2.2.1-beta: Hide grid, axes, markers in Minecraft mode
    // Grid
    {
        graphics::RenderOptions opts;
        opts.pointSize = 1.0f;
        renderer_.record(cmd, gridMesh_.get(), lineMaterial_.get(), swapchain_->extent(), mvp, opts);
    }
    
    // Distance Markers
    if (distanceMarkersMesh_) {
        graphics::RenderOptions opts;
        opts.pointSize = 1.0f;
        renderer_.record(cmd, distanceMarkersMesh_.get(), lineMaterial_.get(), swapchain_->extent(), mvp, opts);
    }
    
    // Axes (with optional animation)
    {
        float axesMVP[16];
        
        if (animationEnabled_) {
            // Get animated transform
            float modelMat[16];
            axesAnimator_.transform().toMatrix(modelMat);
            
            // Multiply: projection * view * model
            float vm[16];
            mulMat4(view, modelMat, vm);
            mulMat4(proj, vm, axesMVP);
        } else {
            // No animation, use regular MVP
            std::copy(mvp, mvp + 16, axesMVP);
        }
        
        graphics::RenderOptions opts;
        opts.pointSize = 1.0f;
        renderer_.record(cmd, axesMesh_.get(), lineMaterial_.get(), swapchain_->extent(), axesMVP, opts);
    }
    
    // Render Finite World if active
    // Render Finite World
    if (finiteRenderer_) {
         std::array<float, 16> mvpArray;
         std::copy(std::begin(mvp), std::end(mvp), mvpArray.begin());

         finiteRenderer_->render(cmd, mvpArray, swapchain_->extent(), 
          /* slope */ showSlopeAnalysis_,
    /* drainage */ showDrainage_, drainageIntensity_,
    /* watershed */ showWatershedVis_, showBasinOutlines_, showSoilVis_,
    /* soil whitelist */ soilHidroAllowed_, soilBTextAllowed_, soilArgilaAllowed_, soilBemDesAllowed_, soilRasoAllowed_, soilRochaAllowed_, 
    /* visual */ sunAzimuth_, sunElevation_, fogDensity_, lightIntensity_); // v3.8.1
    }

    ui::UiFrameContext uiCtx{
        running_,
        needsRecreate_,
        vsyncEnabled_,
        limitIdleFps_,
        fpsCapEnabled_,
        fpsCapTarget_,
        animationEnabled_,
        // showVegetation removed
        camera_,
        // terrain_ removed
        /* finiteMap=*/ finiteMap_.get(),
        /* showSlope */ showSlopeAnalysis_,
        /* showDrainage */ showDrainage_,
        /* intensity */ drainageIntensity_, // [NEW]
        /* showErosion */ showErosion_,
        /* showWatershed */ showWatershedVis_,
        /* showBasinOutlines */ showBasinOutlines_,
        /* showSoilVis */ showSoilVis_,
        /* soilWhitelist */ soilHidroAllowed_, soilBTextAllowed_, soilArgilaAllowed_, soilBemDesAllowed_, soilRasoAllowed_, soilRochaAllowed_,
        /* visual state */ sunAzimuth_, sunElevation_, fogDensity_, // v3.7.3
        // Voxel stats removed
        axesAnimator_,
        bookmarks_,
        lastSurfaceInfo_,
        lastSurfaceValid_,
        lastSurfaceColor_,
        /* seeding & resolution */ currentSeed_, worldResolution_, // v3.7.8
        /* light */ lightIntensity_, // v3.8.1
        /* async */ isRegenerating_ // v3.8.3
    };

    uiLayer_->render(uiCtx, cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {syncObjects_->imageAvailable(frameIndex)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];
    VkSemaphore signalSemaphores[] = {syncObjects_->renderFinished(frameIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(ctx_->graphicsQueue(), 1, &submitInfo, syncObjects_->inFlight(frameIndex));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchainsTmp[] = {swapchain_->handle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchainsTmp;
    presentInfo.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(ctx_->graphicsQueue(), &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
       needsRecreate_ = true;
    }
}


std::unique_ptr<graphics::Material> Application::createMaterial(VkRenderPass renderPass, VkExtent2D extent, VkPrimitiveTopology topology, VkPolygonMode polygonMode) {
    auto vs = std::make_shared<graphics::Shader>(*ctx_, "shaders/basic.vert.spv");
    auto fs = std::make_shared<graphics::Shader>(*ctx_, "shaders/basic.frag.spv");
    return std::make_unique<graphics::Material>(*ctx_, renderPass, extent, vs, fs, topology, polygonMode);
}



void Application::requestTerrainReset(int warmupRadius) {
    // Voxel reset removed. Finite World does not use this warmth reset mechanism currently.
    (void)warmupRadius; 
}

// --- Bookmark System (v3.2.2) ---

void Application::saveBookmark(const std::string& name) {
    ui::Bookmark bookmark;
    bookmark.name = name.empty() ? ("Bookmark " + std::to_string(bookmarks_.size() + 1)) : name;
    bookmark.position = camera_.getPosition();
    bookmark.mode = camera_.getCameraMode();
    
    // Get camera orientation (yaw/pitch) - for Free Flight mode
    // For now, we'll store default values; ideally we'd get these from camera
    bookmark.yaw = 0.0f;
    bookmark.pitch = 0.0f;
    
    bookmarks_.push_back(bookmark);
    std::cout << "[Bookmarks] Saved: " << bookmark.name << " at (" 
              << bookmark.position.x << ", " << bookmark.position.y << ", " << bookmark.position.z << ")" 
              << std::endl;
}

void Application::loadBookmark(size_t index) {
    if (index >= bookmarks_.size()) {
        std::cout << "[Bookmarks] Invalid index: " << index << std::endl;
        return;
    }
    
    const auto& bookmark = bookmarks_[index];
    
    // Switch to bookmark's camera mode if different
    if (camera_.getCameraMode() != bookmark.mode) {
        camera_.setCameraMode(bookmark.mode);
    }
    
    // Teleport to bookmark position
    camera_.teleportTo(bookmark.position);
    
    std::cout << "[Bookmarks] Loaded: " << bookmark.name << std::endl;
}

void Application::deleteBookmark(size_t index) {
    if (index >= bookmarks_.size()) {
        std::cout << "[Bookmarks] Invalid index for delete: " << index << std::endl;
        return;
    }
    
    std::string name = bookmarks_[index].name;
    bookmarks_.erase(bookmarks_.begin() + static_cast<long>(index));
    std::cout << "[Bookmarks] Deleted: " << name << std::endl;
}

// V3.5.0: Map Regeneration
// V3.5.0: Map Regeneration
void Application::regenerateFiniteWorld(const terrain::TerrainConfig& config) {
    if (isRegenerating_) {
        std::cout << "[SisterApp] Regeneration already in progress, request ignored." << std::endl;
        return;
    }
    
    deferredConfig_ = config;
    regenRequested_ = true;
    std::cout << "[SisterApp] Finite generation requested via struct (Deferred): " << config.width << "x" << config.height << std::endl;
}

void Application::performMeshUpdate() {
    if (!finiteRenderer_ || !finiteMap_) return;

    // Safety: Wait for all GPU operations to finish before destroying the old mesh
    // This prevents "CS rejected" or "Device Lost" errors due to use-after-free
    vkDeviceWaitIdle(ctx_->device());

    std::cout << "[SisterApp] Performing deferred mesh update..." << std::endl;
    finiteRenderer_->buildMesh(*finiteMap_, worldResolution_);
    
    meshUpdateRequested_ = false;
}

void Application::performRegeneration() {
    // Phase 1: Start Async Task
    if (regenRequested_ && !isRegenerating_) {
        std::cout << "[SisterApp] Starting Async Regeneration: " << deferredConfig_.width << "x" << deferredConfig_.height << std::endl;
        
        // Capture parameters locally to avoid race conditions if variables change
        // We can just copy the config struct now!
        terrain::TerrainConfig config = deferredConfig_;

        regenRequested_ = false;
        isRegenerating_ = true;

        regenFuture_ = std::async(std::launch::async, [=, this]() {
            // 1. Create independent resources
            auto map = std::make_unique<terrain::TerrainMap>(config.width, config.height);
            auto gen = std::make_unique<terrain::TerrainGenerator>(config.seed);

            // 2. Setup Config (Already copied)
            // Explicitly ensure internal consistency if needed (e.g. width/height)
            // Config is passed entirely.


            // 3. Generate High-Cost Data
            gen->generateBaseTerrain(*map, config);
            gen->calculateDrainage(*map);
            gen->classifySoil(*map, config);

            // 4. Prepare Mesh Data (CPU Heavy)
            auto meshData = shape::TerrainRenderer::generateMeshData(*map, config.resolution);

            // 5. Output to Background Members (Thread Safe? No, member access needs care.)
            // Since main thread checks "future.valid/ready" and doesn't touch these until then, 
            // we can just write them here. But wait, `this` capture.
            // Better: Return a struct and using shared_ptr or move? 
            // Easier: Write to members. Main thread is NOT reading them yet.
            this->backgroundMap_ = std::move(map);
            this->backgroundMeshData_ = std::move(meshData);
            this->backgroundConfig_ = config;
        });

        return; // Return immediately to keep UI running
    }

    // Phase 2: Check Completion
    if (isRegenerating_) {
        // Check if ready (non-blocking)
        if (regenFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            regenFuture_.get(); // Retrieve result (rethrows exceptions)
            
            std::cout << "[SisterApp] Async Generation Finished. Uploading to GPU..." << std::endl;

            // Safe to touch GPU now
            vkDeviceWaitIdle(ctx_->device());

            // Swap Maps
            finiteMap_ = std::move(backgroundMap_);
            currentSeed_ = backgroundConfig_.seed;
            // deferredRegenResolution_ = backgroundConfig_.resolution; // deleted
            worldResolution_ = backgroundConfig_.resolution;

            // Recreate Renderer
            finiteRenderer_.reset();
            finiteRenderer_ = std::make_unique<shape::TerrainRenderer>(*ctx_, swapchain_->renderPass());
            
            // Upload Mesh (Fast Transfer)
            finiteRenderer_->uploadMesh(backgroundMeshData_);
            
            // Clean up heavy data
            backgroundMeshData_.vertices.clear();
            backgroundMeshData_.vertices.shrink_to_fit();
            backgroundMeshData_.indices.clear();
            backgroundMeshData_.indices.shrink_to_fit();

            // Update Minimap
            if (uiLayer_) {
                uiLayer_->onTerrainUpdated(*finiteMap_, backgroundConfig_);
            }

            // Teleport
            float cx = (static_cast<float>(finiteMap_->getWidth()) / 2.0f) * worldResolution_;
            float cz = (static_cast<float>(finiteMap_->getHeight()) / 2.0f) * worldResolution_;
            float h = finiteMap_->getHeight(finiteMap_->getWidth()/2, finiteMap_->getHeight()/2);
            camera_.teleportTo({cx, h + 20.0f, cz});

            isRegenerating_ = false;
            std::cout << "[SisterApp] World Updated!" << std::endl;
        }
    }
}

} // namespace core
