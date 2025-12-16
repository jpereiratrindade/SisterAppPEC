# SisterApp Engine v3.4.0 (Minecraft Mode) 🎮

**SisterApp Engine** is now a high-performance Minecraft-style voxel world exploration tool! This version transforms the scientific visualization engine into an immersive first-person voxel environment with procedural terrain, greedy meshing, and optimized multithreaded loading.

![Version](https://img.shields.io/badge/version-3.6.3-orange.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)
![Status](https://img.shields.io/badge/Status-Stable-green.svg)

---

## 🎯 What's New (v3.5.0)

- **Finite World Mode**: Transition to high-fidelity terrain maps (up to 2048x2048) with realistic generation.
- **Regenerator UI**: Real-time terrain creation with configurable size and smoothness.
- **Drainage Analysis (D8)**: Deterministic flow accumulation visualization (Blue line networks).
- **Visual Overhaul**: Distance Fog, Hemispheric Lighting, and Procedural Noise texturing.
- **Performance**: Device Local memory for meshes ensures stability at 4 Million vertices.
- **Erosion Simulation**: Hydraulic erosion engine (beta).
-   **Watershed Analysis**: Global segmentation and interactive basin delineation (v3.6.3).
-   **Semantic Soil**: CPU-authoritative soil classification ensuring 100% Visual-PROBE consistency (v3.7.3).
-   **Eco-Reports**: Automated generation of hydrological stats (TWI, Drainage Density).

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
- **Tools → Performance**: Real-time control of Draw Distance, Budgets, Models, and Resilience parameters. Toggling **Vegetation** off changes Grass to Dirt for visual clarity.

---

### Slope Analysis (v3.4.0+)
Replaces the previous Ecological Resilience model. This system classifies terrain based on its local inclination (Percentage):
- **Flat (0-3%)**: Plains suitable for construction or agriculture.
- **Gentle Slope (3-8%)**: Transitional areas.
- **Rolling/Ondulado (8-20%)**: Moderate slopes requiring earthworks.
- **Steep Slope (20-45%)**: Challenging terrain, high erosion risk.
- **Mountain (>45%)**: Inaccessible or extreme terrain.

These thresholds are fully configurable via the user interface and persisted between sessions. (Trees, Grass) influenced by moisture and fertility simulations.

- **Social Resilience (`resSoc`)**:
  - Simulates **human impact** and connectivity.
  - Generates "Social Corridors" (paths/clearings) based on connectivity logic.
  - High resilience = Improved infrastructure (flatter paths), organized settlements.

### 2. Vegetation Model
A biological simulation layer driven by:
- **Moisture & Temperature**: Derived from Perlin noise and height.
- **Fertility**: Dynamic factor affecting plant growth rates and density.
- **Visual Feedback**:
  - **Vegetation ON**: Lush green grass, forest canopies.
  - **Vegetation OFF**: Terrain turns to Barren Dirt, trees are removed (simulating drought/collapse).

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
-   [x] **v3.3.0**:
    -   [x] Refactored Core Architecture (No bridges).
    -   [x] Optimized Chunk Scheduling (Frustum + Distance).
    -   [x] Robust Vegetation Toggling (Safe Resource Destruction).
-   [x] **v3.4.0**: Slope Analysis, Persistence, Standardized Versioning.
    -   [ ] Advanced Texturing (Splatting).
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
