#include "application.h"
#include "../graphics/geometry_utils.h"
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
#include <iterator>

namespace core {

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
    
    // Voxel material for terrain (v3.2.2.1-beta)
    {
        auto vs = std::make_shared<graphics::Shader>(*ctx_, "shaders/voxel.vert.spv");
        auto fs = std::make_shared<graphics::Shader>(*ctx_, "shaders/voxel.frag.spv");
        voxelMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), vs, fs,
                                                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
        // Correctly initialize water material with blending enabled and depth write disabled
        waterMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), vs, fs,
                                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                                              /*enableBlend*/true, /*depthWrite*/false);
    }

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
    graphics::createSkyDome(skyVerts, skyIndices, 100.0f, 32);
    skyDomeMesh_ = std::make_unique<graphics::Mesh>(*ctx_, skyVerts, skyIndices);
    
    // NEW: Distance Markers
    std::vector<graphics::Vertex> markersVerts;
    std::vector<uint16_t> markersIndices;
    graphics::createDistanceMarkers(markersVerts, markersIndices, 50, 10);
    distanceMarkersMesh_ = std::make_unique<graphics::Mesh>(*ctx_, markersVerts, markersIndices);
    
    // --- V3.5.0: Finite World Initialization ---
    // Toggle via member var or preference. For now, hardcoded true.
    if (useFiniteWorld_) {
        std::cout << "[SisterApp v3.5.0] Initializing Finite World (1024x1024)..." << std::endl;
        finiteMap_ = std::make_unique<terrain::TerrainMap>(1024, 1024);
        finiteGenerator_ = std::make_unique<terrain::TerrainGenerator>(12345);
        
        // Pass the correct RenderPass!
        finiteRenderer_ = std::make_unique<shape::TerrainRenderer>(*ctx_, swapchain_->renderPass());
        
        terrain::TerrainConfig config;
        config.maxHeight = 50.0f; // Debug: Flatten terrain to isolate spikes
        finiteGenerator_->generateBaseTerrain(*finiteMap_, config);
        finiteGenerator_->applyErosion(*finiteMap_, 500000); 
        
        finiteRenderer_->buildMesh(*finiteMap_);
        
        camera_.setCameraMode(graphics::CameraMode::FreeFlight);
        float cx = 1024.0f / 2.0f;
        float cz = 1024.0f / 2.0f;
        float h = finiteMap_->getHeight(static_cast<int>(cx), static_cast<int>(cz));
        camera_.teleportTo({cx, h + 20.0f, cz}); // Low flight, immersive
        // camera_.lookAt({cx, h, cz}); // Removed, user controls view
        
        std::cout << "[SisterApp v3.5.0] Finite World Ready!" << std::endl;
    } else {
        // V3.2.2.5-beta: Initialize voxel terrain
        terrain_ = std::make_unique<graphics::VoxelTerrain>(*ctx_, 12345);
        terrain_->setFrameFences(&syncObjects_->inFlightFences());
        showVegetation_ = true;
        voxelScene_ = std::make_unique<VoxelScene>(terrain_.get(), voxelMaterial_.get(), waterMaterial_.get());
        voxelScene_->setRenderer(&renderer_);
        voxelStats_ = {};

        // Start in Free Flight mode for voxel world exploration
        camera_.setCameraMode(graphics::CameraMode::FreeFlight);
        // Spawn above terrain height at origin so scene is immediately visible
        int h = terrain_->getTerrainHeight(0, 0);
        camera_.teleportTo({0.0f, static_cast<float>(h) + 8.0f, 0.0f});
        std::cout << "[" << APP_NAME << " " << APP_VERSION_TAG << "] Voxel Terrain Initialized - Minecraft Mode!" << std::endl;
    }
    // End of Finite/Voxel Branch
    // Common setup continues below...

    // V3.4.0: Load preferences on startup
    core::Preferences::instance().load();
    if (terrain_) {
        terrain_->setSlopeConfig(core::Preferences::instance().getSlopeConfig());
    }

    ui::Callbacks uiCallbacks{
        [this](const std::string& name) { saveBookmark(name); },
        [this](size_t index) { loadBookmark(index); },
        [this](size_t index) { deleteBookmark(index); },
        [this](int warmupRadius) { requestTerrainReset(warmupRadius); },
        [this]() { // savePreferences
            if (terrain_) {
                core::Preferences::instance().setSlopeConfig(terrain_->getSlopeConfig());
            }
            core::Preferences::instance().save();
        },
        [this]() { // loadPreferences
            core::Preferences::instance().load();
            if (terrain_) {
                terrain_->setSlopeConfig(core::Preferences::instance().getSlopeConfig());
            }
            // Optionally force regen or let user decide via UI
        }
    };
    uiLayer_ = std::make_unique<ui::UiLayer>(uiCallbacks);
    uiLayer_->applyTheme(ui::Theme::Dark);

    camera_.setAspect(static_cast<float>(swapchain_->extent().width) / static_cast<float>(swapchain_->extent().height));
}

void Application::cleanup() {
    // Wait for GPU work to finish before tearing down resources the command buffers might reference
    if (ctx_) {
        vkDeviceWaitIdle(ctx_->device());
    }

    // Terrain contains Vulkan buffers that must be destroyed while device is valid
    terrain_.reset();
    
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
    voxelMaterial_.reset();        // v3.2.2.1-beta  
    waterMaterial_.reset();        // transparent water
    environmentMaterial_.reset();  // NEW: Environment shaders
    wireframeMaterial_.reset();
    solidMaterial_.reset();
    pointMaterial_.reset();
    lineMaterial_.reset();
    
    syncObjects_.reset(); // Fences/Semaphores
    
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
        std::uint64_t frameStart = SDL_GetPerformanceCounter();
        double deltaSeconds = static_cast<double>(frameStart - prevCounter) / freq;
        prevCounter = frameStart;

        double currentTime = static_cast<double>(SDL_GetTicks()) / 1000.0;
        
        // Idle Check: If no input for 5 seconds and Window is not minimized
        bool isIdle = (limitIdleFps_ && (currentTime - inputManager_.lastInputSeconds() > 5.0));

        processEvents(deltaSeconds);
        update(deltaSeconds);
        
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
            
            // Terrain probe on Left Click (Free Flight)
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (voxelScene_ && terrain_ && camera_.getCameraMode() == graphics::CameraMode::FreeFlight) {
                    float w = static_cast<float>(swapchain_->extent().width);
                    float h = static_cast<float>(swapchain_->extent().height);
                    math::Vec3 rayDir = camera_.getRayDirection(static_cast<float>(event.button.x), static_cast<float>(event.button.y), w, h);
                    math::Ray ray{camera_.getPosition(), rayDir};
                    if (voxelScene_->probeSurface(ray, 600.0f, lastSurfaceInfo_, lastSurfaceValid_)) {
                        std::cout << "[Probe] " << lastSurfaceInfo_ << std::endl;
                    }
                }
            }
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
    auto voxVs = std::make_shared<graphics::Shader>(*ctx_, "shaders/voxel.vert.spv");
    auto voxFs = std::make_shared<graphics::Shader>(*ctx_, "shaders/voxel.frag.spv");
    voxelMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), voxVs, voxFs,
                                                           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
    // Water: blending enabled, depth write disabled to avoid overwriting solids
    waterMaterial_ = std::make_unique<graphics::Material>(*ctx_, swapchain_->renderPass(), swapchain_->extent(), voxVs, voxFs,
                                                           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                                           /*enableBlend*/true, /*depthWrite*/false);
    if (voxelScene_) {
        voxelScene_->setMaterials(voxelMaterial_.get(), waterMaterial_.get());
    }
}
void Application::update(double dt) {
    Uint32 nowMs = SDL_GetTicks();
    // Process keyboard input for camera movement
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    camera_.processKeyboard(keyState, static_cast<float>(dt));

    if (voxelScene_) {
        voxelScene_->applyPendingReset(nowMs, 300);
        voxelScene_->update(dt, camera_, static_cast<int>(currentFrame_));
    } else {
        camera_.update(static_cast<float>(dt));
    }
    
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
    if (terrain_) {
        terrain_->setFrameFences(&syncObjects_->inFlightFences());
    }

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
    int visibleCount = 0;
    int totalChunks = 0;
    int pendingTasks = 0;
    int pendingVeg = 0;
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

    // Sky Dome disabled - was blocking view of terrain
    /*
    if (skyDomeMesh_) {
        graphics::RenderOptions opts;
        opts.pointSize = 1.0f;
        opts.useLighting = true;
        renderer_.record(cmd, skyDomeMesh_.get(), environmentMaterial_.get(), swapchain_->extent(), mvp, opts);
    }
    */

    // V3.2.2.1-beta: Hide grid, axes, markers in Minecraft mode
    if (!terrain_) {
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
    }
    
    // V3.2.2.1-beta: Render Voxel Terrain Chunks with Frustum Culling
    // VoxelScene render needs updating or we need to check if it depends on RenderContext.
    // Assuming voxelScene->render needs update too, but checking header first.
    // Wait, voxelScene passes vk_ to render. Need to check VoxelScene.h next.
    // Assuming VoxelScene uses RenderContext in signature. 
    // I will pass `renderer_` (which is stateless mostly) but `voxelScene` might call `renderer_.record`.
    // VoxelScene::render likely calls renderer. Let's assume for now I need to update VoxelScene too.
    // For this ReplaceFileContent call, I will pass `vk_` as placeholder if needed? No, I must fix VoxelScene.
    // I will call voxelScene_->render(cmd, view, proj, camera_, voxelStats_, swapchain_->extent()); 
    // I need to update VoxelScene signature as well.
    // For now, let's put the call assuming the new signature for VoxelScene::render.
    
    if (useFiniteWorld_ && finiteRenderer_) {
        // v3.5.0 Render Path
        std::array<float, 16> mvpArray;
        std::copy(std::begin(mvp), std::end(mvp), mvpArray.begin());
        finiteRenderer_->render(cmd, mvpArray, swapchain_->extent());
        
        // Also render axes/grid if needed? Maybe not for realistic view.
        // Sky dome could be useful here.
    } else if (voxelScene_) {
        voxelScene_->render(cmd, view, proj, camera_, voxelStats_, swapchain_->extent());
        visibleCount = voxelStats_.visibleChunks;
        totalChunks = voxelStats_.totalChunks;
        pendingTasks = voxelStats_.pendingTasks;
        pendingVeg = voxelStats_.pendingVeg;
    }

    ui::UiFrameContext uiCtx{
        running_,
        needsRecreate_,
        vsyncEnabled_,
        limitIdleFps_,
        fpsCapEnabled_,
        fpsCapTarget_,
        animationEnabled_,
        showVegetation_,
        camera_,
        terrain_.get(),
        visibleCount,
        totalChunks,
        pendingTasks,
        pendingVeg,
        axesAnimator_,
        bookmarks_,
        lastSurfaceInfo_,
        lastSurfaceValid_
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
    if (voxelScene_) {
        voxelScene_->requestReset(warmupRadius);
    }
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

} // namespace core
