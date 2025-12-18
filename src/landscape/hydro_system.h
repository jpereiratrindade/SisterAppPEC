#pragma once

#include "landscape_types.h"
#include "../vegetation/vegetation_types.h"

// Forward Declaration
namespace terrain { class TerrainMap; }

namespace landscape {

    class HydroSystem {
    public:
        // Pre-compute Topology (Slope, Flow Directions, Sort Order)
        // Must be called once or whenever Terrain Height changes.
        static void initialize(HydroGrid& grid, const terrain::TerrainMap& terrain);

        // Dynamic Update: Rain -> Infiltration -> Runoff -> Erosion
        static void update(HydroGrid& grid, 
                           SoilGrid& soil, 
                           const vegetation::VegetationGrid& veg, 
                           float rainRate, // mm/h
                           float dt);      // seconds
    };

} // namespace landscape
