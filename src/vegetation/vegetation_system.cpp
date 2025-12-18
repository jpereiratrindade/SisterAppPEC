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
            
            // Generate correlated noise for patches (FBM)
            // Octave 1
            float n1 = smoothNoise(x * 0.02f, y * 0.02f, seed);
            // Octave 2
            float n2 = smoothNoise(x * 0.1f, y * 0.1f, seed + 999);
            
            float noiseEI = n1 * 0.7f + n2 * 0.3f; // Mixed
            
            // EI (Grass): High coverage base, modulated by noise
            grid.ei_coverage[idx] = 0.6f + 0.4f * (noiseEI * 0.5f + 0.5f); // Normalize roughly
            
            // ES (Shrub): Clumped distribution
            // Use similar FBM but shifted
            float esN1 = smoothNoise(x * 0.02f + 100, y * 0.02f + 100, seed);
            float esN2 = smoothNoise(x * 0.1f + 100, y * 0.1f + 100, seed + 888);
            float noiseES = esN1 * 0.7f + esN2 * 0.3f;
            
            if (noiseES > 0.2f) { // Lower threshold for FBM
                grid.es_coverage[idx] = (noiseES - 0.2f) * 1.5f; 
                if (grid.es_coverage[idx] > 1.0f) grid.es_coverage[idx] = 1.0f;
            } else {
                grid.es_coverage[idx] = 0.0f;
            }
            
            // Vigor: Also varies spatially
            float vigorNoise = smoothNoise(x * 0.1f, y * 0.1f, seed + 55); // Just medium freq for vigor
            grid.ei_vigor[idx] = 0.8f + 0.2f * vigorNoise; 
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
        // Compute capacity based on position
        int x = i % grid.width;
        int y = i / grid.width;
        
        // FBM (Fractal Brownian Motion) for natural patches
        // Octave 1: Low Freq, High Amp
        float n1 = smoothNoise(x * 0.02f, y * 0.02f, 12345);
        // Octave 2: Mid Freq, Mid Amp
        float n2 = smoothNoise(x * 0.1f, y * 0.1f, 67890);
        
        float capacityNoise = n1 * 0.7f + n2 * 0.3f; // Mixed
        
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
            
            // VIGOR DYNAMICS (Simulated seasonality/stress)
            // Instead of always forcing to 1.0, we target a variable level based on noise/time
            float targetVigor = 0.8f + 0.2f * n2; // Vigor varies 0.8-1.0 spatially
            
            // Decay or Recover towards target
            if (grid.ei_vigor[i] < targetVigor) {
                grid.ei_vigor[i] += 0.1f * dt;
            } else if (grid.ei_vigor[i] > targetVigor) {
                grid.ei_vigor[i] -= 0.05f * dt; // Slow decay
            }
            grid.ei_vigor[i] = std::max(0.0f, std::min(1.0f, grid.ei_vigor[i]));

            // Shrubs (ES) recovers slowly if EI is healthy
            // Limit ES based on EI presence?
            // Actually, keep it simpler:
            if (grid.ei_coverage[i] > 0.8f && grid.es_coverage[i] < 1.0f) {
                grid.es_coverage[i] += 0.01f * dt; // 100 seconds to full
                if (grid.es_coverage[i] > 1.0f) grid.es_coverage[i] = 1.0f;
            }
        } // End Recovery Logic

        // Competition Logic: ES suppresses EI (Shading/Space)
        // If ES grows, it pushes EI out.
        if (grid.es_coverage[i] > 0.0f) {
            float availableSpace = 1.0f - grid.es_coverage[i];
            if (grid.ei_coverage[i] > availableSpace) {
                grid.ei_coverage[i] = availableSpace;
            }
        }
    } // End Parallel For Loop
    
    // Explicit invariant enforcement pass (vectorized/parallel friendly)
    enforceInvariants(grid);
}

void VegetationSystem::enforceInvariants(VegetationGrid& grid) {
    if (!grid.isValid()) return;
    int size = static_cast<int>(grid.getSize());

    #pragma omp parallel for
    for (int i = 0; i < size; ++i) {
        // Invariant 1: EI + ES <= 1.0
        float total = grid.ei_coverage[i] + grid.es_coverage[i];
        if (total > 1.0f) {
            // Prioritize ES (Structural) over EI (Opportunistic)
            // Shrink EI to fit
            float excess = total - 1.0f;
            grid.ei_coverage[i] -= excess;
            if (grid.ei_coverage[i] < 0.0f) grid.ei_coverage[i] = 0.0f;
        }
    }
}

void VegetationSystem::applyDisturbance(VegetationGrid& grid, const DisturbanceRegime& regime) {
    if (!grid.isValid()) return;
    
    int size = static_cast<int>(grid.getSize());
    int affectedCount = static_cast<int>(size * regime.spatialExtent); // Simple random selection
    
    // Disturbance Logic
    for (int k = 0; k < affectedCount; ++k) {
         int idx = rand() % size; // Simple random for now
         
         if (regime.type == DisturbanceType::Fire) {
             // Ecological Fire Logic (v3.9.2):
             // High flammability = High ES Biomass AND Low Vigor (Dry fuel).
             // EI (Grass) burns easier but has less fuel load.
             
             float flammability = 0.0f;
             
             // Contribution from Dry Shrub (Primary Fuel according to user feedback)
             if (grid.es_coverage[idx] > 0.2f) {
                 // Flammability increases as Vigor decreases (e.g., Vigor < 0.5)
                 float dryness = std::max(0.0f, 1.0f - grid.es_vigor[idx]);
                 // Non-linear response to dryness
                 if (dryness > 0.5f) { // If vigor < 0.5
                      flammability += grid.es_coverage[idx] * (dryness * 2.0f); 
                 }
             }
             
             // Contribution from Grass (Fine fuel)
             // Grass is flammable even with moderate vigor, but less intense
             flammability += grid.ei_coverage[idx] * 0.3f * (1.0f - grid.ei_vigor[idx]);

             // Stochastic Ignition threshold based on flammability
             // Base probability + Flammability factor
             float prob = 0.05f + flammability * 0.8f; 
             
             if ((float)rand() / RAND_MAX < prob) {
                 // FIRE EVENT!
                 // Total removal of biomass
                 grid.ei_coverage[idx] = 0.0f;
                 grid.es_coverage[idx] = 0.0f;
                 grid.ei_vigor[idx] = 0.0f; // Ash/Blackened
                 grid.es_vigor[idx] = 0.0f;
                 grid.recovery_timer[idx] = regime.averageRecoveryTime; 
             }
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



} // namespace vegetation
