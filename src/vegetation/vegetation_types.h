#pragma once

#include <vector>
#include <cstdint>

namespace vegetation {

    enum class DisturbanceType {
        Fire,
        Grazing,
        Drought,
        None
    };

    // Value Object: Represents the parameters of a disturbance event
    struct DisturbanceRegime {
        DisturbanceType type = DisturbanceType::None;
        float magnitude = 0.0f;          // 0.0 to 1.0 (Biomass removal)
        float frequency = 0.0f;          // Events per time unit (probability)
        float spatialExtent = 0.0f;      // Scale of disturbance (0.0 to 1.0)
        
        // Extended Params for UI/Simulation
        float fireFrequency = 0.0f;      // Specific to Fire
        float grazingIntensity = 0.0f;   // Specific to Grazing
        float averageRecoveryTime = 10.0f; // Seconds

        // Functional Response Coefficients
        float alpha = 10.0f;   // EI (Grass) sensitivity to disturbance (Logarithmic gain)
        float beta = 5.0f;     // ES (Shrub) sensitivity to disturbance (Exponential decay)
        
        // Runtime / Computed
        float calculated_disturbance_index = 0.0f; // D = M * F * E
    };

    // Data Structure: Struct of Arrays (SoA) for cache-friendly processing
    // Stores the state of the vegetation for the entire terrain grid.
    struct VegetationGrid {
        // Dimensions
        int width = 0;
        int height = 0;

        // Data Arrays (aligned for potential SIMD usage)
        // Index = y * width + x
        
        // Lower Stratum (Estrato Inferior - EI) e.g., Grass
        std::vector<float> ei_coverage; // [0.0 - 1.0]
        std::vector<float> ei_vigor;    // [0.0 - 1.0] (Health/Greenness)
        std::vector<float> ei_capacity; // [0.0 - 1.0] Max Capacity (Cached Noise)

        // Upper Stratum (Estrato Superior - ES) e.g., Shrubs/Trees
        std::vector<float> es_coverage; // [0.0 - 1.0]
        std::vector<float> es_vigor;    // [0.0 - 1.0]
        
        // Ecological Memory / Hysteresis
        // Usage: Counts down time until recovery begins, or accumulates stress
        std::vector<float> recovery_timer; 

        // Helpers
        void resize(int w, int h) {
            width = w;
            height = h;
            size_t size = static_cast<size_t>(w * h);
            
            // Initialization Logic (Avoid Bias: Start with mixed state or pure EI depending on design)
            // For now, initializing with full Grass (EI) and no Shrubs (ES) as baseline.
            // Complex initialization should be done by TerrainGenerator/VegetationSystem.
            ei_coverage.assign(size, 1.0f); 
            es_coverage.assign(size, 0.0f); 
            ei_vigor.assign(size, 1.0f);
            es_vigor.assign(size, 1.0f);
            recovery_timer.assign(size, 0.0f);
            ei_capacity.assign(size, 1.0f); // Default full capacity
        }

        size_t getSize() const { return ei_coverage.size(); }
        
        bool isValid() const {
             return !ei_coverage.empty() && 
                    ei_coverage.size() == es_coverage.size() &&
                    width * height == static_cast<int>(ei_coverage.size());
        }
    };

} // namespace vegetation
