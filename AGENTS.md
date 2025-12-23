# AGENTS.md - SisterAppPEC Context

This file serves as a guide for AI Agents and Developers working on the **SisterAppPEC** codebase.

## Project Overview
**SisterAppPEC** is a C++ ecosystem simulation engine focusing on:
- **Terrain Generation**: Voxel-based and finite map generation (`src/terrain`, `graphics/voxel_terrain.cpp`).
- **Landscape Modeling**: Implements ecological systems like Soil (SCORPAN model), Hydro (Drainage), and Vegetation (`src/landscape`).
- **Rendering**: Custom Vulkan engine (`src/graphics`) with SDL2 windowing and ImGui for UI.

## Key Architectures

### Soil System (SCORPAN)
The soil system (`src/landscape/soil_system.cpp`) implements a variation of the SCORPAN model:
- **S (Soil)**: State Vector (Sand, Clay, Organic Matter, Water, etc.).
- **C (Climate)**: Rainfall, Seasonality.
- **O (Organisms)**: Vegetation pressure.
- **R (Relief)**: Slope, Curvature (derived from TerrainMap).
- **P (Parent Material)**: Lithology properties (Weathering rates, base sand/clay bias).
- **A (Age)**: Time evolution ($dt$).
- **N (Space)**: Spatial noise and position-dependent factors.

### Terrain
- **Finite World**: Fixed-size maps (e.g., 1024x1024) used for detailed simulation.
- **Voxel World**: Large-scale terrain (legacy/hybrid).

## Build & Run
- Build System: CMake (implied).
- Platforms: Linux (Primary).

## Documentation
- `docs/` contains detailed mathematical models and technical manuals.
- `docs/manual_tecnico_modelos.md`: Main reference for mathematical implementation.

## Recent Changes (v4.5.x)
- Unified Soil Interface (Geometric vs SCORPAN).
- Integration of Machine Learning models (`src/ml`).
