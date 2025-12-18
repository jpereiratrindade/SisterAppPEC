#pragma once

#include "landscape_types.h"

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
    };

} // namespace landscape
