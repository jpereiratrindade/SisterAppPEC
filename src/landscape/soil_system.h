#pragma once

#include "landscape_types.h"
#include "soil_services.h" // Logic Layer

// Forward declaration to avoid circular dependency
namespace terrain {
    class TerrainMap;
}

namespace landscape {

    class SoilSystem {
    public:
        // Semantic initialization based on Terrain features (Slope, Height)
        static void initialize(SoilGrid& grid, int seed, const terrain::TerrainMap& terrain);

        // Update loop (Compaction, Erosion integration placeholder)
        static void update(SoilGrid& grid, float dt);

        // v4.5.2: Evaluation with external drivers
        // v4.5.2: Evaluation with external drivers (Time-Sliced)
        static void update(SoilGrid& grid, 
                           float dt, 
                           const Climate& climate, 
                           const OrganismPressure& pressure,
                           const ParentMaterial& parent,
                           const terrain::TerrainMap& terrain,
                           int startRow = -1, int endRow = -1);
    };

} // namespace landscape
