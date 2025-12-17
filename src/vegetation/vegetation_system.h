#pragma once

#include "vegetation_types.h"

namespace vegetation {

    class VegetationSystem {
    public:
        // Main simulation step: updates recovery, growth, and invariants
        static void update(VegetationGrid& grid, float dt);

        // v3.9.0: Initialize with spatial heterogeneity
        static void initialize(VegetationGrid& grid, int seed);

        // Apply a disturbance event (e.g., Fire, Grazing)
        // regime.spatialExtent determines the % of cells affected (randomly for now)
        static void applyDisturbance(VegetationGrid& grid, const DisturbanceRegime& regime);

    private:
        // Domain Logic
        static void processRecovery(VegetationGrid& grid, float dt);
        static void enforceInvariants(VegetationGrid& grid);
    };

} // namespace vegetation
