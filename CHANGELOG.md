# Changelog

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
