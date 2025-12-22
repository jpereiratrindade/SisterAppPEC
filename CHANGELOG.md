# Changelog

## [v4.3.12] - 2025-12-22
### Documentation
- **Theoretical Manual**: Deep dive into *scorpan* Topographic Model, detailing the specific slope heuristic ($\max \Delta H$) and exact threshold values for soil classification.

## [v4.3.11] - 2025-12-22
### Documentation
- **Theoretical Manual**: Added `docs/manual_teorico_sistemas.md` detailing the mathematical models behind Soil (scorpan), Hydro (D8/StreamPower), and Vegetation (Logistic Growth).

## [v4.3.10] - 2025-12-22
### Bug Fixes
- **Documentation**: Fixed Mermaid diagram syntax in `manual_tecnico_ml.md` (escaped parentheses in node labels).

## [v4.3.9] - 2025-12-22
### UI Improvements
- **Dedicated ML Hub**: Extracted Machine Learning controls from "Soil" inspector into a new, dedicated "Machine Learning Hub" inspector.
- **Toolbar Update**: Added specific button for "ML Service" (Purple) to elevate its visibility in the interface.
- **Refactor**: Cleaned up Soil Inspector to focus solely on Pedology and DDD patterns.

## [v4.3.8] - 2025-12-22
### Documentation
- **ML Integration Manual**: Created `docs/manual_tecnico_ml.md` detailing the "Hub & Spoke" architecture and model catalog.
- **Architecture**: Adjusted `README.md` to link to the new ML manual.

## [v4.3.7] - 2025-12-22
### Documentation
- **Architecture Flowcharts**: Added Mermaid diagrams to `README.md` and `manual_tecnico_modelos.md` to visualize system architecture and data flow.
- **Manual Structure**: Renumbered sections in the Technical Manual for consistency.

## [v4.3.6] - 2025-12-22
### Pattern Integrity Lab Alianmnent
- **Refined Validation Logic**: Implemented "PatternIntegrityLab" adjustments to improve ecological realism.
    - **Relative Normalization**: Validation metrics (LSI/CF) now calculate deviation relative to the envelope range (`max - min`) rather than absolute max, increasing sensitivity for restrictive soil types.
    - **Fuzzy Circularity**: Breaks in RCC envelopes now contribute to `Under Tension` state instead of causing immediate `Incompatible` failure.
    - **New State "In Transition"**: Activated detection of mixed signals (e.g., Stable LSI + Unstable CF) or asymmetric stress, classifying them as transitional landscapes rather than errors.
    - **Ecological Resolution**: Renamed "Too Small" feedback to "Below Ecological Resolution" to better reflect the epistemological choice of ignoring noise.
- **Documentation**: Updated `manual_tecnico_modelos.md` Section 4 with new states and theoretical justifications.


## [v4.3.5] - 2025-12-22
### Added
- **Configurable Pattern Envelopes:** Added UI in "Soil & ML" inspector to adjust LSI, CF, and RCC validation ranges at runtime.
- **Auto-Fix Stability:** Button to automatically apply terrain parameters (Scale 0.0010, Persistence 0.40) that guarantee stable patterns.
- **Noise Tolerance:** Validator now ignores patches < 50 pixels.

### Changed
- **Default Terrain:** Updated default noise scale to 0.0010 and persistence to 0.40.
- **Validation Logic:** Relaxed default envelopes for LSI.

## [v4.3.0] - 2025-12-22
### UI Overhaul
- **Toolbar + Inspector Architecture**: Completely replaced the floating window system with a unified persistent Toolbar and a context-sensitive Inspector panel.
    - **Toolbar**: Quick access to "Terrain", "Hydrology", "Soil & ML", and "Vegetation" modes.
    - **Inspector**: Dynamic right-side panel that displays controls relevant to the active tool.
    - **Resizeable/Movable**: Inspector panel can now be resized and moved by the user.
- **ML UI Feedback**: Added real-time dataset size counters and training status indicators to the Soil & ML Inspector.

## [v4.2.1] - 2025-12-19
### New Features
- **ML Phase 2: Disturbance & Growth**: 
    - Integrated "fire_risk" and "biomass_growth" ML models into `MLService`.
    - Multi-model real-time inference in Terrain Probe.
- **Advanced ML Hub**:
    - Unified UI panel with dynamic hyperparameter control (Epochs, Learning Rate).
    - User-defined sampling size for dataset collection.

## [v4.2.0] - 2025-12-19
### New Features
- **Generic ML Service**: Refactored `MLService` to support multiple named models (e.g., "soil_color", "hydro_runoff") with generic input vectors.
- **Hydrology ML Model**: Implemented "hydro_runoff" model including stochastic training data generation (randomized rain events).
- **Windowed Mode**: Removed forced fullscreen. App now launches in a resizeable 1600x900 window by default.
- **Generic Training API**: Unified training pipeline for all perceptron models.

## [v4.1.1] - 2025-12-19
### Bug Fixes
- **Soil System**: Fixed initialization bug where soil depth was stuck at default 1.0m (Landscape generation call was missing).
- **Hydro Probe**: Fixed "Runoff" displaying 0.0 (was reading unused variable `water_depth`, switched to `flow_flux`).
- **Erosion**: Adjusted erosion threshold from `1e-4` to `1e-9` to correctly visualize micro-erosion at small time steps.

## [v4.1.0] - 2025-12-19
### UI Refactoring (Domain-Driven UI)
- **Split "Map Generator"**: Refactored the monolithic tool window into 4 domain-specific floating windows:
    - **Terrain & Environment**: Generation parameters, lighting, fog, and slope analysis.
    - **Hydrology Tools**: Rain intensity, drainage visualization, and watershed segmentation.
    - **Soil & ML**: Soil map filters and Machine Learning controls.
    - **Vegetation Tools**: Disturbance settings (Fire/Grazing) and visualization modes.
- **Main Menu**: Added "Domain Windows" menu to toggle these windows individually.
- **Goal**: Align UI with DDD aggregates and reduce visual clutter.

## [v4.0.0] - 2025-12-18
### Integrated Landscape Model
- **Major Architecture Assessment**: Transitioned from isolated systems to a fully coupled **Soil-Hydro-Vegetation** feedback loop.
- **Components**:
    - **Hydro System**: Implemented D8 Flow Routing, Dynamic Runoff ($P - I$), and Stream Power Erosion.
    - **Soil System**: Implemented dynamic Soil Depth ($d$), Propagule Bank ($\Pi$), and Organic Matter ($OM$).
    - **Vegetation System**: Implemented Site Quality ($d \times OM$) feedback on Carrying Capacity ($K$) and Water Stress feedback on Vigor ($\phi$).
- **Documentation**: 
    - Updated Manual Técnico with Section 4 "Modelo Integrado Ecofuncional".
    - Verified all DDD invariants in validation testing.
    - Updated Manual Técnico with Section 4 "Modelo Integrado Ecofuncional".
    - Verified all DDD invariants in validation testing.
### Machine Learning (Experimental)
- **Experimental ML Integration**: Added `MLService` and `Perceptron` for soil color prediction (PoC).
    - **Physics-Guided Training**: Implemented in-game data collection and asynchronous training loop to prevent UI freezes.
    - **UI Updates**: Added "ML Training" controls and "Accurate Soil Color" toggle.
    - **Documentation**: Added Section 4.7 to Manual detailing ML architecture.
    - Integrated Eigen-based Multi-Layer Perceptron (MLP).
    - Added `MLService` for loading and managing model lifecycles.
    - Implemented hybrid rendering where soil color is predicted from physical properties ($d$, $OM$, $Inf$, $Comp$).
    - Added "Accurate Soil Color" toggle in Map Generator UI.

### Ecological Modeling
- **Functional Response to Disturbance**: 
    - Implemented logarithmically positive response for Grass ($R_{EI}$) and exponentially negative response for Shrubs ($R_{ES}$).
    - Disturbance is now a composite index $D = M \cdot F \cdot E$ (Magnitude, Frequency, Extent).
    - Vegetation carrying capacity is now dynamic, modulated by $D$.
- **Robustness & Optimization**: 
    - Replaced `rand()` with `std::mt19937` for deterministic simulations.
    - Pre-computed Capacity Noise in `initialize()` to reduce per-frame CPU load.
    - Implemented **Facilitation**: Shrubs (`ES`) now require `EI > 0.7` to grow.
    - Added numerical clamping to prevent infinite growth.

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

