# SisterApp Engine v3.3.0 (Minecraft Mode) 🎮

**SisterApp Engine** is now a high-performance Minecraft-style voxel world exploration tool! This version transforms the scientific visualization engine into an immersive first-person voxel environment with procedural terrain, greedy meshing, and optimized multithreaded loading.

![Version](https://img.shields.io/badge/version-3.3.0-orange.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)
![Status](https://img.shields.io/badge/Status-Stable-green.svg)

---

## 🎯 What's New (v3.3.0)

- **Optimized Scheduling**: Chunk generation and meshing now prioritize visible chunks (Frustum Culling), vastly improving responsiveness when moving fast.
- **Architectural Refactor**: Simplified core by removing legacy bridges. Now using a unified `GraphicsContext` and `VoxelScene` architecture.
- **Greedy Meshing**: Optimized mesh generation for solids and water, with transparent pass and correct winding.
- **Improved Vegetation**: Thread-safe vegetation toggling/regeneration without crashes.
- **Metrics Overlay**: Detailed stats including "Chunks Visible/Total" and prioritized task queues.

---

## 🛠️ Dependencies

-   **Vulkan SDK** (1.2+)
-   **SDL2** (Window/Input)
-   **Eigen3** (Linear Algebra)
-   **Dear ImGui** (UI - included)

---

## 🔧 Build Instructions

### Linux

```bash
# Install dependencies (Fedora/RHEL)
sudo dnf install vulkan-devel SDL2-devel eigen3-devel

# Install dependencies (Ubuntu/Debian)
sudo apt install libvulkan-dev libsdl2-dev libeigen3-dev

# Build
cmake -S . -B build
cmake --build build

# Run
./build/sisterapp
```

### macOS

```bash
# Install dependencies
brew install vulkan-sdk sdl2 eigen

# Build and run
cmake -S . -B build
cmake --build build
./build/sisterapp
```

---

## 🎮 Controls

### Movement
- **W/A/S/D**: Move forward/left/backward/right
- **Space**: Jump (only when on ground) or Fly Up (Creative Mode)
- **Mouse**: Look around (right-click + drag or toggle mode)
- **Shift**: Run (3x speed boost)
- **Alt**: Slow motion (0.3x speed)
- **Z / X**: Roll left/right (Free Flight)
- **[ / ]**: Adjust FOV (45–110°)
- **LMB**: Probe terrain info

### Camera
- **Tab/C**: Toggle camera mode (Free Flight / Orbital)
- **Tools Menu**: Enable creative flight for no-clip movement.

### Shortcuts
- **R**: Reset to spawn
- **1-4**: Quick Teleport
- **F5-F8**: Bookmarks (Save/Load)
- **Ctrl+T**: Toggle Theme

### Application
- **Tools → Performance**: Real-time control of Draw Distance, Budgets, Models, and Resilience parameters.

---

## 🌍 World Generation

The terrain uses **multi-octave Perlin noise** with selectable profiles:
- **Plano**: Gentle rolling plains.
- **Suave (Default)**: Balanced hills and valleys.
- **Ondulado**: Higher peaks and deeper valleys.
- **Biomes**: Currently single-biome with procedural vegetation (Trees, Grass) influenced by moisture and fertility simulations.

---

## 🚀 Technical Features

### Rendering
- **Vulkan 1.3** pipeline.
- **Prioritized Greedy Meshing** (Visible chunks mesh first).
- **Transparent Water Pass**.
- **Per-Face Ambient Occlusion**.
- **Frustum Culling** for both rendering and generation/meshing queues.

### Architecture
- **Unified Context**: `core::GraphicsContext` manages Vulkan device state.
- **Scene Isolation**: `VoxelScene` encapsulates game logic, cleaner `Application` loop.
- **Multithreading**: Worker pool for background chunk generation vs Main thread rendering.

---

## 📊 Performance

On modern hardware (e.g., RX 7900 XTX / Ryzen 9):
- **FPS**: 120+ (can be capped in Tools).
- **Loading**: Instant response to camera turns due to prioritization.
- **Draw Calls**: Minimized via Greedy Meshing and Culling.

---

## 🚀 Roadmap

-   [x] **v3.2.x**: Bookmarks, Stats, Themes, Initial Voxel Terrain.
-   [x] **v3.3.0-beta**:
    -   [x] Refactored Core Architecture (No bridges).
    -   [x] Optimized Chunk Scheduling (Frustum + Distance).
    -   [x] Robust Vegetation Toggling.
-   [ ] **v3.4.0**: PCoA Data Visualization restoration.
-   [ ] **v3.5.0**: Multiplayer exploration / Block interaction.
-   [ ] **v4.0.0**: VR Support.

---

## 📜 License

Provided as-is for research and educational purposes.

---

## 👨‍💻 Author

**José Pedro Trindade**  
_Voxel World Exploration & Visualization_

---

**Explore infinite procedural worlds in 3D! 🌍🎮**
