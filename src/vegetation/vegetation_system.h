#pragma once

#include "vegetation_types.h"

#include <vector>

// Forward decls for coupling
namespace landscape {
    struct SoilGrid;
    struct HydroGrid;
}

namespace vegetation {

    class VegetationSystem {
    public:
        // Initialize state (coverage, biomass)
        static void initialize(VegetationGrid& grid, int seed);

        // Update loop (Growth, Competition, Facilitation)
        // v4.0: Integrated Landscape (Soil/Hydro Coupling)
        static void update(VegetationGrid& grid, 
                           float dt, 
                           const DisturbanceRegime& disturbance,
                           const landscape::SoilGrid* soil = nullptr,
                           const landscape::HydroGrid* hydro = nullptr);

        // Apply a disturbance event (e.g., Fire, Grazing)
        // regime.spatialExtent determines the % of cells affected (randomly for now)
        static void applyDisturbance(VegetationGrid& grid, const DisturbanceRegime& regime);

    private:
        // Domain Logic
        static void processRecovery(VegetationGrid& grid, float dt);
        static void enforceInvariants(VegetationGrid& grid);
    };

} // namespace vegetation
