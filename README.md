# SisterApp Engine v3.9.2
Plataforma de Ecologia Computacional

> **De Engine Gráfica para Laboratório Digital**: V3.8.0 transforma o projeto em uma plataforma científica para simulação de processos ecológicos, geração de padrões espaciais e validação métrica.

SisterApp é uma **Plataforma de Ecologia Computacional** de alto desempenho (C++/Vulkan) focada em simular dinâmicas de paisagem (Hidrologia, Pedologia, Geomorfologia) e validar hipóteses ecológicas através de métricas espaciais rigorosas (LSI, CF, RCC).

Desenvolvido por **José Pedro Trindade**.

![Version](https://img.shields.io/badge/version-3.9.2-orange.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)
![Status](https://img.shields.io/badge/Status-Stable-green.svg)

---

## 🎯 What's New (v3.9.2)

-   **High-Scale Vegetation**: Optimized vegetation simulation for large-scale maps (4096^2) via throttled updates (v3.9.2).
-   **Vegetation Dynamics (DDD)**: Grass/Shrub competition with Fire and Grazing disturbance regimes (v3.9.1).

-   **Computational Ecology Platform**: Complete rebranding from "Engine" to "Scientific Platform".
-   **Interactive Minimap**: Real-time navigation aid with peak detection (symbols), zoom, and click-to-teleport.
-   **Landscape Metrics**: Integration of LSI (Shape), CF (Complexity), and RCC (Circularity) for validation of soil patch distribution.
-   **Water Level Control**: Adjustable sea-level for terrain generation, preventing "all-blue" maps.
-   **Finite World Mode**: High-fidelity terrain maps (up to 2048x2048) with realistic generation (v3.5+).
-   **Drainage Analysis (D8)**: Deterministic flow accumulation visualization (v3.6+).
-   **Semantic Soil**: CPU-authoritative soil classification ensuring 100% Visual-PROBE consistency (v3.7+).
-   **Deterministic Seeding**: Full control over map seeds for reproducible terrain analysis (v3.7+).
-   **Experimental Blend (Finite)**: Multi-frequency terrain blending ported to Finite World generator (v3.8.3).
-   **Eco-Reports**: Automated generation of hydrological and landscape stats (v3.7+).

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
- **Mouse Wheel**: Zoom (Adjust FOV)
- **[ / ]**: Fine Tune FOV (45–110°)
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
- **Tools → Map Generator**: Real-time control of Terrain Size, Water Level, Roughness, and Seed.
- **Tools → Visuals**: Sun position, Render Distance (Fog), and Minimap toggle.

---

## 🌿 Scientific Models

### 1. Slope Analysis (Geometric)
Classifies terrain based on local inclination (Percentage):
- **Flat (0-3%)**: Plains suitable for construction or agriculture.
- **Gentle Slope (3-8%)**: Transitional areas.
- **Rolling/Ondulado (8-20%)**: Moderate slopes requiring earthworks.
- **Steep Slope (20-45%)**: Challenging terrain, erosion risk.
- **Mountain (>45%)**: Inaccessible terrain.

### 2. Pedology (Landscape Ecology)
Soil distribution is simulated using spatially coherent noise patterns inspired by Landscape Ecology principles (Farina, 2006):
- **LSI (Landscape Shape Index)**: Controls patch edge complexity via Domain Warping.
- **CF (Complexity Factor)**: Controls internal roughness via Fractal Octaves.
- **RCC (Related Circumscribing Circle)**: Controls patch elongation via Anisotropy.
- **Classes**: Hidromórfico, B Textural, Argila Expansiva, Bem Desenvolvido, Raso, Rocha.

### 3. Hydrology (D8)
Deterministic O(N) flow accumulation algorithm:
- Calculates flow direction based on steepest descent.
- Accumulates flux from ridge lines to valleys.
- Visualizes drainage networks (Flux > Threshold).
- Segments terrain into drainage basins (Watersheds).

---

## 🚀 Roadmap

-   [x] **v3.5.0**: Finite World & Slope Analysis.
-   [x] **v3.6.0**: D8 Drainage & Watersheds.
-   [x] **v3.7.0**: Semantic Soil & Landscape Metrics.
-   [x] **v3.8.0**: Minimap, Rebranding, & Stability.
    -   [ ] **v3.x**: Scientific Visualization (NDVI Synthesis & Stress Signaling).
    -   [ ] Advanced Texturing (Splatting).
-   [ ] **v4.0.0**: VR Support & Agent Simulation.

---

## 📜 License

Provided as-is for research and educational purposes.

---

## 👨‍💻 Author

**José Pedro Trindade**  
_Voxel World Exploration & Visualization_

---

**Explore infinite procedural worlds in 3D! 🌍🎮**
