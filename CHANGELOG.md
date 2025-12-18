# Changelog

## [v3.9.4] - 2025-12-18
### Visualization
- **Standardized NDVI**: Updated NDVI visualization to use scientific color palette (Brown/Yellow/Green) and standardized range (0.1 - 0.9).
- **NDVI Probe**: Added real-time simulated NDVI value readout to the terrain probe.

## [v3.9.3] - 2025-12-18
### Performance
- **Parallel Computing**: Enabled OpenMP support to utilize all CPU cores for vegetation simulation and texture generation (16-32x speedup on high-end CPUs).
- **Optimization**: Implemented "Capacity Caching" in `VegetationSystem` to eliminate expensive procedural noise recalculations (16 million ops/frame -> 0).
- **Memory Management**: Implemented Persistent Staging Buffer in `TerrainRenderer` to prevent 64MB per-frame allocation stalls.
- **Async Upload**: Fixed asynchronous GPU transfer synchronization using `VkFence` to prevent CPU blocking.

## [v3.9.2] - 2025-12-18
### Optimized
- **Vegetation Performance**: Implemented simulation throttling (~5-10Hz) to debottleneck CPU-GPU bandwidth.
  - Reduced update frequency from Per-Frame to Fixed-Timestep (200ms default).
  - Added performance profiling logging (`[Perf]`) to monitor simulation vs upload times.
  - Solved 1-2 FPS issue on large maps (4096+).
- Solved 1-2 FPS issue on large maps (4096+).

### Ecological Accuracy
- **Refined Fire Logic**: Flammability is now driven by dry Upper Stratum (High Biomass + Low Vigor) rather than random probability.
- **Ecological Invariants**: Updated DDD to enforce that "ES becomes flammable primarily when senescent".

### Documentation
- **Visual DDD**: Formalized concepts for **NDVI Synthesis** and **Visual Stress Signaling** in `DDD_Representacao_Vegetacao_Campestre.md`.
- **Manual**: Clarified Upper Stratum definition as "Sub-shrubs (< 1m)" typical of pastoral environments.
- **Reference**: Updated `DDD.md` to explicitly link the new Vegetation Domain documents.
## [v3.9.1] - 2025-12-17
### Added
- **Grassland Vegetation Model (DDD)**:
    - Implemented Two-Stratum system (Grass/Shrub) with competitive interactions.
    - Added Spatially Heterogeneous initialization using noise patterns.
    - Added Disturbance Regimes: **Fire** (Total removal, stochastic) and **Grazing** (Selective Grass removal).
    - Added `VegetationSystem` for managing growth, recovery, and disturbances.
- **Visualization**:
    - New Renderer support for dynamic vegetation texture uploads.
    - Added Visualization Modes: Realistic, Heatmap EI (Grass), Heatmap ES (Shrub), Vigor.
    - Integrated with Soil/Terrain shaders for seamless blending.
- **UI**:
    - Added "Vegetation Model" section in Tools.
    - Added controls for Fire Frequency, Grazing Intensity, and Recovery Time.
    - Added **"Reset Vegetation"** button to re-initialize heterogeneity without regenerating terrain.

### Fixed
- **Visual Monotony**: Fixed initialization bug where vegetation started uniformly green. Now initializes with patchy, natural structure.
- **Regeneration Bug**: Fixed issue where map regeneration resulted in homogeneous vegetation due to missing initialization call in async thread.
- **Texture Stretch**: Fixed `TerrainRenderer` bug where vegetation texture wasn't resizing with the map, causing 1-pixel stretch.

## [v3.8.4] - 2025-12-11
### Removed
- **Voxel System**: Completely removed Voxel terrain code, shaders, and UI dependencies to focus on Finite World.

### Fixed
- **Build System**: Cleaned up CMakeLists.txt.
- **UI**: Fixed ImGui assertions and disabled stack errors.

## [v3.8.3] - 2025-12-11
### Added
- **Experimental Blend Model**: New terrain generation mode combining 3 noise frequencies.
- **Async Regeneration**: Moved map generation to background thread to prevent UI freezing.

