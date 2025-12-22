#include "soil_system.h"
#include "lithology_registry.h"
#include "../terrain/terrain_map.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace landscape {

    // Helper: Simple slope calculation
    float calculateSlope(int x, int y, const terrain::TerrainMap& terrain) {
        int w = terrain.getWidth();
        int h = terrain.getHeight();
        float H = terrain.getHeight(x, y);
        
        float maxDiff = 0.0f;
        // Check 4 neighbors
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};
        
        for(int k=0; k<4; ++k) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if(nx >=0 && nx < w && ny >= 0 && ny < h) {
                float diff = std::abs(terrain.getHeight(nx, ny) - H);
                if(diff > maxDiff) maxDiff = diff;
            }
        }
        return maxDiff; // Approximation
    }

    void SoilSystem::initialize(SoilGrid& grid, int seed, const terrain::TerrainMap& terrain) {
        if (!grid.isValid()) return;
        
        int w = grid.width;
        int h = grid.height;
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        #pragma omp parallel for collapse(2)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = y * w + x;
                float slope = calculateSlope(x, y, terrain);
                
                // Pedology Rules (Simplified)
                // Steep slope (> 2.0 height diff) -> Thin soil, Rocky
                // Flat -> Deep soil
                
                if (slope > 2.0f) {
                    grid.depth[i] = 0.1f + dis(gen) * 0.2f; // Shallow [0.1 - 0.3m]
                    grid.soil_type[i] = static_cast<uint8_t>(SoilType::Rocky);
                    grid.infiltration[i] = 10.0f; // Low intilfration (Runoff high)
                    grid.propagule_bank[i] = 0.2f; // Hard to regenerate
                } else if (slope > 0.5f) {
                    grid.depth[i] = 0.5f + dis(gen) * 0.5f; // Medium [0.5 - 1.0m]
                    grid.soil_type[i] = static_cast<uint8_t>(SoilType::Sandy_Loam);
                    grid.infiltration[i] = 80.0f; // Good drainage
                    grid.propagule_bank[i] = 0.8f;
                } else {
                    grid.depth[i] = 1.0f + dis(gen) * 1.0f; // Deep [1.0 - 2.0m]
                    grid.soil_type[i] = static_cast<uint8_t>(SoilType::Clay_Loam); // Deposition area
                    grid.infiltration[i] = 40.0f; // Slower drainage (Retention)
                    grid.propagule_bank[i] = 1.0f; // Rich bank
                }

                // Organic Matter correlates with depth
                grid.organic_matter[i] = std::min(1.0f, grid.depth[i] * 0.5f);
                grid.compaction[i] = 0.0f; // Fresh start
            }
        }
    }

    void SoilSystem::update(SoilGrid& grid, float dt) {
        // Placeholder for dynamic compaction recovery or simple weathering
        // For Phase 1, soil is static after initialization (except if modified by Hydro later)
        (void)grid;
        (void)dt;
    }

} // namespace landscape
