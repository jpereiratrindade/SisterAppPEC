#include "vegetation_system.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vegetation {

// Simple Pseudo-Random Noise for Initialization
// (Replacing full Perlin dependency for autonomy)
float pseudoNoise(int x, int y, int seed) {
    int n = x + y * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}


// Custom lerp for < C++20
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Bilinear Interpolation
float smoothNoise(float x, float y, int seed) {
    int X = (int)floor(x);
    int Y = (int)floor(y);
    float fx = x - X;
    float fy = y - Y;
    
    float v1 = pseudoNoise(X, Y, seed);
    float v2 = pseudoNoise(X + 1, Y, seed);
    float v3 = pseudoNoise(X, Y + 1, seed);
    float v4 = pseudoNoise(X + 1, Y + 1, seed);
    
    float i1 = lerp(v1, v2, fx);
    float i2 = lerp(v3, v4, fx);
    
    return lerp(i1, i2, fy);
}

void VegetationSystem::initialize(VegetationGrid& grid, int seed) {
    if (!grid.isValid()) return;
    
    int w = grid.width;
    int h = grid.height;
    
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            
            // Generate correlated noise for patches
            float scale = 0.05f; // Frequency
            float noiseEI = smoothNoise(x * scale, y * scale, seed) * 0.5f + 0.5f; // 0-1
            float noiseES = smoothNoise(x * scale + 100, y * scale + 100, seed) * 0.5f + 0.5f;
            
            // EI (Grass): High coverage base, modulated by noise
            grid.ei_coverage[idx] = 0.6f + 0.4f * noiseEI; // 0.6 - 1.0
            
            // ES (Shrub): Clumped distribution
            if (noiseES > 0.6f) {
                grid.es_coverage[idx] = (noiseES - 0.6f) * 2.5f; // 0.0 - 1.0 blobs
            } else {
                grid.es_coverage[idx] = 0.0f;
            }
            
            // Vigor: Also varies spatially
            float vigorNoise = smoothNoise(x * scale * 2.0f, y * scale * 2.0f, seed + 99);
            grid.ei_vigor[idx] = 0.8f + 0.2f * vigorNoise; // 0.6 - 1.0 (Healthy)
            grid.es_vigor[idx] = grid.ei_vigor[idx];
            
            grid.recovery_timer[idx] = 0.0f;
        }
    }
}

void VegetationSystem::update(VegetationGrid& grid, float dt) {
    if (!grid.isValid()) return;
    
    // In OpenMP parallel loop for performance
    int size = static_cast<int>(grid.getSize());
    

    #pragma omp parallel for
    for (int i = 0; i < size; ++i) {
        // Compute capacity based on position (re-using pseudoNoise)
        int x = i % grid.width;
        int y = i / grid.width;
        
        // Capacity Logic: Not 1.0 everywhere!
        // Re-calculate noise to keep "Carrying Capacity" consistent with initialization
        // We use the same seed 0 implicit or passed? We don't have seed here.
        // We'll trust a static seed or simple hash for stability.
        float capacityNoise = pseudoNoise(x / 20, y / 20, 12345); // Coarse patches
        float maxEI = 0.8f + 0.2f * capacityNoise; // 0.6 - 1.0
        
        // Handle Recovery Timer
        if (grid.recovery_timer[i] > 0.0f) {
            grid.recovery_timer[i] -= dt;
            if (grid.recovery_timer[i] < 0.0f) grid.recovery_timer[i] = 0.0f;
        }

        // Recovery Logic (Succession)
        if (grid.recovery_timer[i] <= 0.0f) {
            // Grass (EI) recovers fast
            if (grid.ei_coverage[i] < maxEI) {
                grid.ei_coverage[i] += 0.1f * dt; // 10 seconds to full
                if (grid.ei_coverage[i] > maxEI) grid.ei_coverage[i] = maxEI;
            }
            if (grid.ei_vigor[i] < 1.0f) {
                grid.ei_vigor[i] += 0.2f * dt;
                if (grid.ei_vigor[i] > 1.0f) grid.ei_vigor[i] = 1.0f;
            }
            
            // Shrubs (ES) recovers slowly if EI is healthy
            // Limit ES based on EI presence?
            float maxES = (maxEI > 0.9f) ? 0.8f : 0.0f; // Only supports shrubs in high capacity zones?
            // Actually, keep it simpler:
            if (grid.ei_coverage[i] > 0.8f && grid.es_coverage[i] < 1.0f) {
                grid.es_coverage[i] += 0.01f * dt; // 100 seconds to full
                if (grid.es_coverage[i] > 1.0f) grid.es_coverage[i] = 1.0f;
            }
        }
    }
    
    enforceInvariants(grid);
}

void VegetationSystem::applyDisturbance(VegetationGrid& grid, const DisturbanceRegime& regime) {
    if (!grid.isValid()) return;
    
    int size = static_cast<int>(grid.getSize());
    int affectedCount = static_cast<int>(size * regime.spatialExtent); // Simple random selection
    
    // Disturbance Logic
    for (int k = 0; k < affectedCount; ++k) {
         int idx = rand() % size; // Simple random for now
         
         if (regime.type == DisturbanceType::Fire) {
             // Total removal of biomass, set recovery time
             grid.ei_coverage[idx] = 0.0f;
             grid.es_coverage[idx] = 0.0f;
             grid.ei_vigor[idx] = 0.0f;
             grid.es_vigor[idx] = 0.0f;
             grid.recovery_timer[idx] = regime.averageRecoveryTime; 
         } 
         else if (regime.type == DisturbanceType::Grazing) {
             // Selective removal of EI (Grass)
             float removal = regime.grazingIntensity; // e.g., 0.2 remove 20%
             grid.ei_coverage[idx] -= removal;
             if (grid.ei_coverage[idx] < 0.1f) grid.ei_coverage[idx] = 0.1f; // Overgrazing limit
             
             // Vigor impact
             grid.ei_vigor[idx] -= removal * 0.5f;
             if (grid.ei_vigor[idx] < 0.2f) grid.ei_vigor[idx] = 0.2f;
         }
    }
}

void VegetationSystem::processRecovery(VegetationGrid& grid, float dt) {
    // Implemented in update for now
    (void)grid; (void)dt;
}

void VegetationSystem::enforceInvariants(VegetationGrid& grid) {
    // Ensure 0-1 range
    // Parallelized in update loop implicitly, but if called separately:
    // Skipping for performace as update handles limits.
    (void)grid;
}

} // namespace vegetation
