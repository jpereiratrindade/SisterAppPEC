# Changelog

## [v3.8.4] - 2025-12-17
### Removed
-   **Voxel Terrain System**: Permanently removed the legacy Voxel engine to focus exclusively on the high-fidelity Finite World implementation.
    -   Deleted `src/graphics/voxel_terrain.*` and `src/voxel_scene.*`.
    -   Removed Voxel shaders and related build configurations.
-   **Legacy Code**: Cleaned up `Application` and `Camera` classes, removing unused variables and methods.

### Changed
-   **Documentation**: Updated Technical Manual and README to reflect the platform's scientific focus.
-   **Camera**: Refactored collision logic to be independent of voxel structures.

## [v3.8.3] - 2025-12-16
### Added
-   **Experimental Blend Terrain (Finite World)**: Ported the multi-frequency blending model from Voxel mode to Finite World generator.
    -   **Controls**: Sliders for Low, Mid, and High frequency weights, plus Exponent for peak sharpness.
    -   **UI Integration**: New "Use Experimental Blend" checkbox in Map Generator.

### Changed
-   **Architecture Refactor**:
    -   Refactored `TerrainConfig` to use nested `BlendConfig` struct and explicit `FiniteTerrainModel` enum.
    -   Separated algorithmic model selection from parameter values.

### Fixed
-   **Terrain Artifacts**: Fixed "flattened tops" (clipping) in blend mode by normalizing noise values against the total weight sum.

## [v3.8.0] - 2025-12-17
### Added
-   **Interactive Minimap**:
    -   Real-time top-down navigation with "Fog of War" style rendering.
    -   **Symbols Allegory**: Automatic detection and visualization of Mountain Peaks (Triangles).
    -   **Controls**: Click-to-teleport, Zoom, and Pan support within the UI window.
    -   **Water Level Viz**: Configurable sea-level visualization to identify submerged terrain.
-   **Camera & Controls**:
    -   **Zoom (FOV)**: Mouse Wheel now controls Field of View in Free Flight mode.
    -   **Water Level Slider**: Added parameter to Terrain Generator to control base water height (default 64m).
    -   **Render Distance**:- **v3.8.3 (Current):** Implemented Async Regeneration (fixes "Not Responding" on large maps), Added Loading Overlay, Optimized Mesh Build.
- **v3.8.2:** Expanded Map Size Options (up to 4096), Improved Map Generator UI (Dimensions Readout).
- **v3.8.1:** Refined Horizon/Fog blending, Fixed Sky Sphere clipping, Implemented Top-Down View, Added Views Menu (incl. Probe & Debug), Disabled Preferences Persistence, Removed Animation Controls.
- v3.8.0: Minimap, Camera Control, Render Distance.
- v3.7.9: Landscape Ecology Patches (Metrics).
-   **Landscape Metrics Validation**: Added tools for quantifying landscape patterns (LSI, CF, RCC).

### Fixed
-   **Stability**: Fixed critical `vkDestroySampler` crash on application shutdown by enforcing correct destruction order of UI resources.
-   **Minimap Sync**: Resolved issue where minimap location/texture did not update after resizing the terrain (e.g., to 2048x2048).

## [v3.7.9] - 2025-12-16
### Added
-   **Landscape Ecology Soil Patches**: Implemented spatially coherent soil distribution based on Farina's descriptors.
    -   **LSI (Shape Index)**: Mapped to Domain Warping to simulate edge complexity.
    -   **CF (Complexity)**: Mapped to Fractal Roughness (Octaves).
    -   **RCC (Circularity)**: Mapped to Noise Anisotropy (Stretching).
-   **Documentation**: Updated Technical Manual with new soil methodology.

## [v3.7.8] - 2025-12-16
### Fixed
-   **Critical Stability**: 
    -   Fixed application crash on shutdown by enforcing `vkDeviceWaitIdle` before resource destruction.
    -   Removed duplicate `syncObjects_` reset causing double-free errors.
-   **D8 Drainage Algorithm**:
    -   Updated flow direction logic to use **Physical Slope** (Drop/Distance) instead of just Height Drop, eliminating cardinal direction bias.
    -   Algorithm now correctly accounts for diagonal distance ($\sqrt{2}$).
-   **Hydrology Report**:
    -   Metrics (TWI, Slope, Drainage Density) now correctly incorporate `resolution` (cell size), reporting physical units (m², m/m).
    -   Signature updated to propagate `worldResolution` from UI to report generator.
-   **Deterministic Seeding**:
    -   Added **Seed Input** to Terrain Generator UI.
    -   Wired `config.seed` to noise generators and soil classification hash, ensuring 100% reproducibility.

## [v3.7.3] - 2025-12-15
### Fixed
-   **Semantic Soil Architecture**: Refactored soil classification to be **CPU-Authoritative**.
    -   **Consistency**: Visual rendering and Probe Logic now share the exact same `SoilType` data source (`TerrainMap::soilMap`), guaranteeing 100% consistency.
    -   **Renderer**: Soil ID is now calculated once during generation and passed to the GPU via a custom Vertex Attribute (`soilId`), removing complex procedural logic from the fragment shader.
    -   **Probe**: Probe tool now reads directly from the semantic map instead of attempting to replicate shader math.

## [v3.6.3] - 2025-12-15
### Added
-   **Watershed Analysis**: New module for drainage basin delineation.
    -   **Global Segmentation**: Partitions the entire terrain into distinct catchment basins.
    -   **Interactive Delineation**: Right-click to trace the upstream area for any point.
    -   **Visualization**: Basin map overlay with flat interpolation for artifact-free boundaries.
-   **Hydrology Report v2**:
    -   Added **Drainage Density** and **Saturated Area %** metrics.
    -   Added **Basin Statistics** section (Count, Largest Area).
    -   Updated terminology ("Potencial Geomorfológico").
-   **UI**: Auto-segmentation trigger when enabling Watershed view.

### Fixed
-   **Crash**: Fixed GPU race condition during mesh rebuilding (implemented deferred updates).
-   **Visual Artifacts**: Fixed "rainbow" interpolation at basin boundaries using `flat` shading.

## [v3.6.6] - 2025-12-15
### Changed
-   **Terrain Generation**: Decoupled roughness from resolution. Noise sampling now uses physical coordinates (`x * resolution`) ensuring consistent terrain shape across different cell sizes.
-   **Configuration**: Added `resolution` field to `TerrainConfig`.

## [v3.6.5] - 2025-12-15
### Added
-   **Spatial Resolution**: Added "Cell Size" slider (0.1m - 2.0m) to control simulation grid density.
-   **Interaction**: Updated raycasting logic to support accurate mouse interaction on scaled terrain meshes.
-   **Docs**: Added "Spatial Resolution" section to Technical Manual.

## [v3.6.4] - 2025-12-15
### Added
-   **Visualization**: Basin Outline rendering ("Show Contours" checkbox) using shader derivatives.
-   **UI**: Helper toggle for outlines inside Watershed tool group.

## [v3.6.0] - 2025-12-15
### Added
-   **Drainage Visualization**: Implemented D8 (Steepest Descent) algorithm for deterministic river network generation.
-   **D8 Algorithm**: Replaced stochastic particle erosion with O(N) flow accumulation.
-   **Visualization**: Bright Cyan shader visualization for accumulated flux.
-   **UI**: Separate toggle for "Show Drainage Model".

### Fixed
-   **Terrain Artifacts**: Resolved "spots"/pitting issues caused by noisy erosion simulations.
-   **Build Stability**: Fixed multiple compilation errors in TerrainGenerator.
-   **Performance**: D8 algorithm is significantly faster than particle simulation.

### Changed
-   **Erosion Model**: Removed "Show Erosion Model" toggle (deprecated in favor of pure drainage analysis).
-   **Docs**: Updated technical manual with D8 algorithm details.

## [v3.5.0] - 2025-12-14
### Added
-   **Finite World**: Added support for non-voxel terrain generation.
-   **Slope Analysis**: Added 5-class slope classification system.
