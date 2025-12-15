#pragma once

#include "../init_sdl.h"

#include "graphics_context.h"
#include "swapchain.h"
#include "command_pool.h"
#include "sync_objects.h"
#include "../renderer.h"
#include "../voxel_scene.h"
#include "../graphics/camera.h"
#include "../graphics/material.h"
#include "../graphics/mesh.h"
#include "../graphics/animation.h"
#include "../math/frustum.h"
#include "../graphics/voxel_terrain.h"
#include "../ui/ui_layer.h"
#include "../ui/bookmark.h"
#include "input_manager.h"
#include "../terrain/terrain_map.h"
#include "../terrain/terrain_generator.h"
#include "../terrain/terrain_renderer.h"
#include <memory>
#include <vector>
#include <string>

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
        void regenerateFiniteWorld(int size, float scale, float amplitude); // v3.5.1
        void performRegeneration(); // v3.5.0 internal


    private:    // --- Core Systems ---
        SDLContext sdl_;
        std::unique_ptr<GraphicsContext> ctx_; // Wrapper for Vulkan Instance/Device
        std::unique_ptr<Swapchain> swapchain_;
        std::unique_ptr<CommandPool> commandPool_;
        std::unique_ptr<SyncObjects> syncObjects_;

        // Direct Management of Per-Frame Resources
        std::vector<VkCommandBuffer> commandBuffers_;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

        // Sync Object Handles (Copied from SyncObjects for fast access, or just use SyncObjects directly)
        // Kept simplier: use wrappers directly or mirror as vector<VkSemaphore> if needed.
        // Let's use the SyncObjects wrapper methods in the loop instead of caching vectors of handles manually.

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
        std::unique_ptr<graphics::Material> voxelMaterial_;  // v3.2.2.1-beta: Voxel terrain
        std::unique_ptr<graphics::Material> waterMaterial_;  // Transparent water

        // Meshes
        std::unique_ptr<graphics::Mesh> gridMesh_;
        std::unique_ptr<graphics::Mesh> axesMesh_;
        std::unique_ptr<graphics::Mesh> skyDomeMesh_;
        std::unique_ptr<graphics::Mesh> distanceMarkersMesh_;
        
        // --- Voxel Terrain (v3.2.2.1-beta) ---
        std::unique_ptr<graphics::VoxelTerrain> terrain_;
        std::unique_ptr<VoxelScene> voxelScene_;
        VoxelSceneStats voxelStats_{};
        bool showVegetation_ = true;
        std::string lastSurfaceInfo_;
        bool lastSurfaceValid_ = false;

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
    std::unique_ptr<terrain::TerrainMap> finiteMap_;
    std::unique_ptr<terrain::TerrainGenerator> finiteGenerator_;
    std::unique_ptr<shape::TerrainRenderer> finiteRenderer_;
    bool useFiniteWorld_ = true; // Toggle for v3.5.0 logic

    // Performance Settings
    bool vsyncEnabled_ = false;  // Default: Off (High FPS)
    bool limitIdleFps_ = true;   // Default: On (Save Power)
    bool fpsCapEnabled_ = true;
    float fpsCapTarget_ = 120.0f;
    // Cap adicional quando VSync está OFF para evitar runaway de frames
    float vsyncOffFpsCap_ = 240.0f;
    InputManager inputManager_;

    // Deferred Actions (v3.5.0 fix)
    // Deferred Actions (v3.5.0 fix)
    bool regenRequested_ = false;
    int deferredRegenSize_ = 1024;
    float deferredRegenScale_ = 0.002f;
    float deferredRegenAmplitude_ = 80.0f; // v3.5.1
    
    // Visualization State
    bool showSlopeAnalysis_ = false; // v3.5.2
    bool showDrainage_ = false; // v3.6.1
    bool showErosion_ = false; // v3.6.2
    };

} // namespace core
