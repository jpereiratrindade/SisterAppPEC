#include "vegetation_system.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

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
    
    // Use std::mt19937 for better randomness if needed, but perlin uses integer hash.
    // We keep the noise functions deterministic based on seed.

    #pragma omp parallel for collapse(2)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            
            // --- 1. EI (Grass) Capacity Initialization ---
            // FBM (Fractal Brownian Motion) for natural patches
            // Octave 1: Low Freq, High Amp
            float n1 = smoothNoise(x * 0.02f, y * 0.02f, seed);
            // Octave 2: Mid Freq, Mid Amp
            float n2 = smoothNoise(x * 0.1f, y * 0.1f, seed + 999);
            
            float capacityNoiseEI = n1 * 0.7f + n2 * 0.3f; // Mixed
            
            // Base Capacity for EI (Space/Soil potential)
            // Range [0.6, 1.0] modulated by noise
            grid.ei_capacity[idx] = 0.6f + 0.4f * (capacityNoiseEI * 0.5f + 0.5f);
            
            // Set initial coverage to full capacity
            grid.ei_coverage[idx] = grid.ei_capacity[idx];


            // --- 2. ES (Shrub) Capacity Initialization ---
            // Independent noise layer for shrubs (patchy, clumped)
            float esN1 = smoothNoise(x * 0.02f + 100, y * 0.02f + 100, seed);
            float esN2 = smoothNoise(x * 0.1f + 100, y * 0.1f + 100, seed + 888);
            float capacityNoiseES = esN1 * 0.7f + esN2 * 0.3f;
            
            // Shrubs are patchier. If noise is low, capacity is zero.
            if (capacityNoiseES > 0.2f) { 
                grid.es_capacity[idx] = (capacityNoiseES - 0.2f) * 1.5f; 
                if (grid.es_capacity[idx] > 1.0f) grid.es_capacity[idx] = 1.0f;
            } else {
                grid.es_capacity[idx] = 0.0f;
            }

            // Initial ES coverage (Start with some, but let it grow)
            grid.es_coverage[idx] = grid.es_capacity[idx] * 0.5f;

            
            // --- 3. Vigor Initialization ---
            float vigorNoise = smoothNoise(x * 0.1f, y * 0.1f, seed + 55); 
            grid.ei_vigor[idx] = 0.8f + 0.2f * vigorNoise; 
            grid.es_vigor[idx] = grid.ei_vigor[idx];
            
            grid.recovery_timer[idx] = 0.0f;
        }
    }
}

void VegetationSystem::update(VegetationGrid& grid, float dt, const DisturbanceRegime& regime) {
    if (!grid.isValid()) return;
    
    // In OpenMP parallel loop for performance
    int size = static_cast<int>(grid.getSize());
    
    // Calculate Global Disturbance Index (Regime-based)
    float D = regime.magnitude * regime.frequency * regime.spatialExtent;
    
    // Calculate Functional Responses
    // EI (Grass): Logarithmic positive response to disturbance
    float R_EI = std::log(1.0f + regime.alpha * D);
    R_EI = std::max(0.0f, std::min(1.0f, R_EI)); // Clamp to [0,1]
    
    // ES (Shrub): Exponential negative response to disturbance
    float R_ES = std::exp(-regime.beta * D);
    R_ES = std::max(0.0f, std::min(1.0f, R_ES)); // Clamp to [0,1]

    #pragma omp parallel for
    for (int i = 0; i < size; ++i) {
        // --- CAPACITY MODULATION (Regime-based) ---
        // Base capacity from cache, scaled by functional response.
        
        // EI (Grass): Resilient but responsive
        float currentMaxEI = grid.ei_capacity[i] * (0.3f + 0.7f * R_EI);
        
        // ES (Shrub): Highly sensitive (Exponential decay)
        float currentMaxES = grid.es_capacity[i] * R_ES;

        // Handle Recovery Timer
        if (grid.recovery_timer[i] > 0.0f) {
            grid.recovery_timer[i] -= dt;
            if (grid.recovery_timer[i] < 0.0f) grid.recovery_timer[i] = 0.0f;
        }

        // --- DYNAMICS (Growth/Dieback) ---
        if (grid.recovery_timer[i] <= 0.0f) {
            // 1. EI (Grass) Dynamics
            if (grid.ei_coverage[i] < currentMaxEI) {
                grid.ei_coverage[i] += 0.1f * dt; // Logistic Growth
                if (grid.ei_coverage[i] > currentMaxEI) grid.ei_coverage[i] = currentMaxEI;
            } else if (grid.ei_coverage[i] > currentMaxEI) {
                grid.ei_coverage[i] -= 0.05f * dt; // Dieback (Competition/Stress)
                 if (grid.ei_coverage[i] < currentMaxEI) grid.ei_coverage[i] = currentMaxEI;
            }
            
            // Vigor (Simulated seasonality)
            // Ideally should also be cached or global parameter
            float targetVigor = 0.8f; 
            if (grid.ei_vigor[i] < targetVigor) {
                grid.ei_vigor[i] += 0.1f * dt;
            } else {
                grid.ei_vigor[i] -= 0.05f * dt; 
            }
            grid.ei_vigor[i] = std::max(0.0f, std::min(1.0f, grid.ei_vigor[i]));
            grid.es_vigor[i] = grid.ei_vigor[i]; // Shared vigor for now

            // 2. ES (Shrub) Dynamics with FACILITATION
            // Rule: Shrubs only grow effectively if grass matrix is established (Microclimate/Soil retention).
            // Threshold EI > 0.7
            bool facilitationActive = grid.ei_coverage[i] > 0.7f;
            
            if (grid.es_coverage[i] < currentMaxES) {
                if (facilitationActive) {
                    grid.es_coverage[i] += 0.02f * dt; // Slow growth
                } else {
                     // No growth (stagnation) or very slow? 
                     // Let's assume stagnation without facilitation.
                }
                
                if (grid.es_coverage[i] > currentMaxES) grid.es_coverage[i] = currentMaxES;
            } else if (grid.es_coverage[i] > currentMaxES) {
                 // Dieback due to disturbance pressure
                 grid.es_coverage[i] -= 0.1f * dt; 
                 if (grid.es_coverage[i] < currentMaxES) grid.es_coverage[i] = currentMaxES;
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
    int affectedCount = static_cast<int>(size * regime.spatialExtent); 
    
    // Modern RNG: Deterministic and high quality
    // Using static to persist state across calls (Sequence continuity)
    static std::mt19937 gen(123456789); 
    std::uniform_int_distribution<> disIndex(0, size - 1);
    std::uniform_real_distribution<> disProb(0.0f, 1.0f);

    // Disturbance Logic
    for (int k = 0; k < affectedCount; ++k) {
         int idx = disIndex(gen); 
         
         if (regime.type == DisturbanceType::Fire) {
             // Ecological Fire Logic (v3.9.2):
             // High flammability = High ES Biomass AND Low Vigor (Dry fuel).
             
             float flammability = 0.0f;
             
             // Contribution from Dry Shrub
             if (grid.es_coverage[idx] > 0.2f) {
                 float dryness = std::max(0.0f, 1.0f - grid.es_vigor[idx]);
                 if (dryness > 0.5f) { 
                      flammability += grid.es_coverage[idx] * (dryness * 2.0f); 
                 }
             }
             
             // Contribution from Grass (Fine fuel)
             flammability += grid.ei_coverage[idx] * 0.3f * (1.0f - grid.ei_vigor[idx]);

             // Stochastic Ignition
             float prob = 0.05f + flammability * 0.8f; 
             
             if (disProb(gen) < prob) {
                 // FIRE EVENT!
                 grid.ei_coverage[idx] = 0.0f;
                 grid.es_coverage[idx] = 0.0f;
                 grid.ei_vigor[idx] = 0.0f; // Ash/Blackened
                 grid.es_vigor[idx] = 0.0f;
                 grid.recovery_timer[idx] = regime.averageRecoveryTime; 
             }
         } 
         else if (regime.type == DisturbanceType::Grazing) {
             // Selective removal of EI (Grass)
             float removal = regime.grazingIntensity; 
             grid.ei_coverage[idx] -= removal;
             if (grid.ei_coverage[idx] < 0.1f) grid.ei_coverage[idx] = 0.1f;
             
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
