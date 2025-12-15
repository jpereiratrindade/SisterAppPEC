# Changelog

## [v3.7.3] - 2025-12-15
### Fixed
-   **Soil Classification**: Fixed discrepancy between visual rendering (GPU) and probe logic (CPU).
    -   **Renderer**: Corrected normal calculation in `TerrainRenderer` to account for `gridScale`. Flat terrain now correctly renders as flat (Purple/Argila) instead of steep (Green/Raso).
    -   **Probe**: Improved noise sampling precision to use exact raycast hit position, ensuring 1:1 match with visual stochastic noise.

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
