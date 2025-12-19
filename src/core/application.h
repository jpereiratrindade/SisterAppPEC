#pragma once

#include "../init_sdl.h"

#include "graphics_context.h"
#include "swapchain.h"
#include "command_pool.h"
#include "sync_objects.h"
#include "../renderer.h"
// VoxelScene Removed
#include "../graphics/camera.h"
#include "../graphics/material.h"
#include "../graphics/mesh.h"
#include "../graphics/animation.h"
#include "../math/frustum.h"
// Voxel Headers Removed
#include "../ui/ui_layer.h"
#include "../ui/bookmark.h"
#include "input_manager.h"
#include "../terrain/terrain_map.h"
#include "../terrain/terrain_generator.h"
#include "../terrain/terrain_renderer.h"
#include "../vegetation/vegetation_types.h"
#include <vector>
#include <memory>
#include <future>
#include <atomic>
#include <chrono>
#include <string>

namespace ml { class MLService; } // Forward decl

namespace core {
    /**
     * @brief The main Engine class encapsulating the application lifecycle.
     * 
     * This class manages the initialization, event loop (SDL), rendering loop (Vulkan),
     * and cleanup of all engine resources. It acts as the central hub for:
     * - Window Management (SDL2)
     * - core::GraphicsContext (Vulkan Instance/Device)
     * - core::Swapchain (Presentation)
     * - Input Processing (Camera, Picking)
     * - UI Rendering (ImGui Overlay)
     */
    class Application {
    public:
        Application();
        ~Application();

        // Prevent copying
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        /**
         * @brief Runs the main application loop.
         * 
         * This method blocks until the application is closed. It executes:
         * 1. processEvents(): Handling SDL events (Input, Resize, UI).
         * 2. update(): Updating game logic/camera.
         * 3. render(): Recording and submitting Vulkan command buffers.
         */
        void run();

    private:
        /** @brief Initialize all subsystems (SDL, Vulkan, ImGui, Assets). */
        void init();
        
        /** @brief The main loop body (legacy, largely replaced by run/processEvents structure). */
        void mainLoop();
        
        /** @brief Release all resources in reverse order of creation. */
        void cleanup();
        
        /** @brief Handle window and input events. */
        void processEvents(double dt);
        
        /** @brief Update per-frame logic (Camera, Animations). */
        void update(double dt);
        
        /** @brief Record commands and present the frame. */
        void render(size_t currentFrame);
        
        /** @brief Handle swapchain recreation on window resize. */
        void recreateSwapchain();

        // Helper to create materials logic (ported from main)
        std::unique_ptr<graphics::Material> createMaterial(VkRenderPass renderPass, VkExtent2D extent, VkPrimitiveTopology topology, VkPolygonMode polygonMode);
        void rebuildMaterials();
        
        void saveBookmark(const std::string& name);
        void loadBookmark(size_t index);
        void deleteBookmark(size_t index);
        void requestTerrainReset(int warmupRadius = 1);
        void regenerateFiniteWorld(const terrain::TerrainConfig& config); // v3.8.3 Struct-based
        void performRegeneration(); // v3.5.0 internal


    private:    // --- Core Systems ---
        SDLContext sdl_;
        std::unique_ptr<GraphicsContext> ctx_; // Wrapper for Vulkan Instance/Device
        // Deferred Actions (v3.5.0 fix)
        bool regenRequested_ = false;
        terrain::TerrainConfig deferredConfig_; // v3.8.3 Consolidating deferred state
        
        float worldResolution_ = 1.0f; // v3.6.5 Current active resolution
        int currentSeed_ = 12345;      // v3.7.8 Current active seed
        std::unique_ptr<Swapchain> swapchain_;
        std::unique_ptr<CommandPool> commandPool_;
        std::unique_ptr<SyncObjects> syncObjects_;

        // Direct Management of Per-Frame Resources
        std::vector<VkCommandBuffer> commandBuffers_;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

        // --- Core Engine Components ---
        graphics::Renderer renderer_;
        graphics::Camera camera_;

        // --- Assets / Materials ---
        // Materials
        std::unique_ptr<graphics::Material> lineMaterial_;
        std::unique_ptr<graphics::Material> pointMaterial_;
        std::unique_ptr<graphics::Material> solidMaterial_;
        std::unique_ptr<graphics::Material> wireframeMaterial_;
        std::unique_ptr<graphics::Material> environmentMaterial_;  // NEW: Enhanced shaders
        std::unique_ptr<graphics::Material> waterMaterial_;  // Transparent water

        // Meshes
        std::unique_ptr<graphics::Mesh> gridMesh_;
        std::unique_ptr<graphics::Mesh> axesMesh_;
        std::unique_ptr<graphics::Mesh> skyDomeMesh_;
        std::unique_ptr<graphics::Mesh> distanceMarkersMesh_;
        
        // --- State ---
        bool running_ = true;
        size_t currentFrame_ = 0;
        bool needsRecreate_ = false;

        // --- UI & Logic State ---

        // --- Animation ---
        graphics::Animator gridAnimator_;     // Animates the grid
        graphics::Animator axesAnimator_;     // Animates the axes
        bool animationEnabled_ = false;       // Toggles animation on/off
        
        // --- Bookmarks (v3.2.2) ---
        std::vector<ui::Bookmark> bookmarks_;
        std::unique_ptr<ui::UiLayer> uiLayer_;
        
        // Safety & Stability (v3.3.0)
        std::vector<VkFence> imagesInFlight_;
        bool wireframeSupported_ = false;

        // --- Finite Terrain System (v3.5.0) ---
        // This is now the ONLY terrain system
        std::unique_ptr<terrain::TerrainMap> finiteMap_;
        std::unique_ptr<terrain::TerrainGenerator> finiteGenerator_;
        std::unique_ptr<shape::TerrainRenderer> finiteRenderer_;

        // Performance Settings
        bool vsyncEnabled_ = false;  // Default: Off (High FPS)
        bool limitIdleFps_ = true;   // Default: On (Save Power)
        bool fpsCapEnabled_ = true;
        float fpsCapTarget_ = 120.0f;
        // Cap adicional quando VSync está OFF para evitar runaway de frames
        float vsyncOffFpsCap_ = 240.0f;
        InputManager inputManager_;

        // Visualization State
        bool showSlopeAnalysis_ = false; // v3.4.0
        bool showDrainage_ = false;      // v3.6.1
        float drainageIntensity_ = 0.5f; // v3.6.1
        bool showErosion_ = false;       // v3.6.2
        bool showWatershedVis_ = false;  // v3.6.3
        bool showBasinOutlines_ = false; // v3.6.4
        bool showSoilVis_ = false;       // v3.7.0
        
        //    // Soil Whitelist
        bool soilHidroAllowed_ = true;
        bool soilBTextAllowed_ = true;
        bool soilArgilaAllowed_ = true;
        bool soilBemDesAllowed_ = true;
        bool soilRasoAllowed_ = true;
        bool soilRochaAllowed_ = true;

        // Visual Controls
        float sunAzimuth_ = 45.0f;
        float sunElevation_ = 60.0f;
        float fogDensity_ = 0.0005f; // v3.8.0 Tweaked for larger view
        float lightIntensity_ = 1.0f; // v3.8.1
        
        // v3.8.3: Async Regeneration
        std::future<void> regenFuture_;
        std::atomic<bool> isRegenerating_{false};
        std::unique_ptr<terrain::TerrainMap> backgroundMap_;
        shape::TerrainRenderer::MeshData backgroundMeshData_;
        terrain::TerrainConfig backgroundConfig_; // Store config for main thread use (Minimap)
        
        // v3.6.3 Deferred Update Flag
        bool meshUpdateRequested_ = false;
        void performMeshUpdate();

        // Probe State (Restored)
        std::string lastSurfaceInfo_;
        bool lastSurfaceValid_ = false;
        float lastSurfaceColor_[3] = {0.0f, 0.0f, 0.0f};

        // v3.9.0 Vegetation State
        int vegetationMode_ = 1; // Default to Realistic (1)
        vegetation::DisturbanceRegime disturbanceParams_; // Default constructor has sensible defaults?
        
        // v3.9.1 Throttle Vegetation Updates
        Uint32 lastVegetationUpdateMs_ = 0;
        Uint32 vegetationUpdateIntervalMs_ = 200; // 5Hz (conservative to break death spiral)
        
        // v4.0: Landscape Integration
        float rainIntensity_ = 50.0f; // mm/h (Heavy Rain for Testing)
     
        // v4.0.0 ML Visualization Toggle
        bool showMLSoil_ = false; // Default OFF for PoC verification

        // ML Service (v4.0 PoC)
        // Forward declared in .h or included? We need include.
        // Actually unique_ptr needs definition if incomplete type used in destructor?
        // But headers are included. Let's rely on forward decl in .h but here include is needed?
        // ml::MLService is not included here. I forward declared it?
        // No, I need to include, or forward declare.
        // I will define it as unique_ptr with forward decl in namespace.
        std::unique_ptr<ml::MLService> mlService_;
        
        // v4.2.1 Advanced ML Controls
        bool isTraining_ = false;
        std::future<void> trainingFuture_;
        
        int mlTrainingEpochs_ = 50;
        float mlLearningRate_ = 0.1f;
        int mlSampleCount_ = 1000;
        
    };

} // namespace core

