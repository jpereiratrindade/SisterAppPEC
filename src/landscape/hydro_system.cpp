#include "hydro_system.h"
#include "../terrain/terrain_map.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <numeric>

namespace landscape {

    void HydroSystem::initialize(HydroGrid& grid, const terrain::TerrainMap& terrain) {
        if (!grid.isValid()) return;
        
        int w = grid.width;
        int h = grid.height;
        size_t size = static_cast<size_t>(w * h);
        
        // 1. Calculate Slopes & Receivers (Topology)
        // Using "Steepest Descent" (D8)
        
        #pragma omp parallel for collapse(2)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = y * w + x;
                float currentH = terrain.getHeight(x, y);
                float maxSlope = -1.0f;
                int bestReceiver = -1;
                
                // Check 8 neighbors
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx==0 && dy==0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            float neighborH = terrain.getHeight(nx, ny);
                            float drop = currentH - neighborH;
                            
                            if (drop > 0) {
                                float dist = (dx==0 || dy==0) ? 1.0f : 1.4142f;
                                float slope = drop / dist; // Physical slope
                                
                                if (slope > maxSlope) {
                                    maxSlope = slope;
                                    bestReceiver = ny * w + nx;
                                }
                            }
                        }
                    }
                }
                
                grid.receiver_index[i] = bestReceiver;
                grid.slope[i] = (maxSlope > 0) ? maxSlope : 0.0f; // Tangent of angle
            }
        }
        
        // 2. Compute Topological Sort Order (High to Low Elevation)
        // This allows O(N) flux accumulation
        std::iota(grid.sort_order.begin(), grid.sort_order.end(), 0);
        
        // Optimize: Sort indices based on height
        // We need direct access to HeightMap to sort efficiently
        // Note: Sort is expensive, do it only on init.
        // It's safer to not parallelize the sort itself with this access pattern unless using parallel sort.
        std::sort(grid.sort_order.begin(), grid.sort_order.end(), [&](int a, int b) {
            // Get coordinates from index
            int ax = a % w; int ay = a / w;
            int bx = b % w; int by = b / w;
            return terrain.getHeight(ax, ay) > terrain.getHeight(bx, by);
        });
    }

    void HydroSystem::update(HydroGrid& grid, SoilGrid& soil, const vegetation::VegetationGrid& veg, float rainRate, float dt) {
        if (!grid.isValid() || !soil.isValid()) return;
        
        size_t size = grid.water_depth.size();
        
        // Convert Rain Rate (mm/h) to Runoff Source Term (m/step)
        // 1 mm = 0.001 m.
        // Step accumulation = (Rain / 3600) * dt
        float rainPerStep = (rainRate * 0.001f / 3600.0f) * dt; 

        // 1. Reset Traient Flux
        std::fill(grid.flow_flux.begin(), grid.flow_flux.end(), 0.0f);
        
        // 2. Calculate Runoff Generation (Source)
        // Parallelizable
        #pragma omp parallel for
        for (int i = 0; i < (int)size; ++i) {
            // Infiltration Capacity (mm/h converted to m/s approx relative)
            // SoilBase * (1 + VegCoeff * Biomass)
            // Veg roots increase porosity.
            
            float biomass = veg.ei_coverage[i] + veg.es_coverage[i]; // Total veg
            float baseInfil = soil.infiltration[i]; // e.g. 50 mm/h
            
            // Effective Infiltration (mm/h)
            float effectiveInfil = baseInfil * (1.0f + biomass * 2.0f); 
            
            // Convert to m per step
            float infilPerStep = (effectiveInfil * 0.001f / 3600.0f) * dt;
            
            // Water Balance
            // If Rain > Infil, Excess = Runoff
            // If Infil > Rain, Soil Moisture increases (not modeled explicitly yet in HydroGrid, implied in Soil)
            
            float runoffSrc = 0.0f;
            if (rainPerStep > infilPerStep) {
                runoffSrc = rainPerStep - infilPerStep;
            } else {
                // All absorbed
                runoffSrc = 0.0f;
            }
            
            grid.flow_flux[i] = runoffSrc; // Initial flux is just local generation
        }

        // 3. route Flow (Serial - Dependency Chain)
        // Iterate from High to Low. Push water to receiver.
        // This CANNOT be parallelized trivially.
        for (int idx : grid.sort_order) {
            int receiver = grid.receiver_index[idx];
            
            if (receiver != -1) {
                grid.flow_flux[receiver] += grid.flow_flux[idx];
            }
            
            // Calculate Erosion here or in a separate pass?
            // Separate pass allows parallelization.
        }
        
        // 4. Erosion / Deposition Logic (Parallel)
        #pragma omp parallel for
        for (int i = 0; i < (int)size; ++i) {
            float flux = grid.flow_flux[i];
            float slope = grid.slope[i];
            
            // Stream Power approx: Flux * Slope
            // Scale factor K for erosion rate
            float K_erod = 5.0f; // Arbitrary scaler for now
            
            // Resistance: Vegetation protects soil
            float protection = (veg.ei_coverage[i] + veg.es_coverage[i] * 1.5f); // Shrubs protect more?
            if (protection > 1.0f) protection = 1.0f;
            
            float resistance = 1.0f - protection * 0.9f; // Max 90% protection
            
            float erosionPot = flux * slope * K_erod * resistance;
            
            // Threshold
            if (erosionPot > 1e-9f) { // Very small threshold (Physics-based)
                 if (soil.depth[i] > 0.0f) {
                     soil.depth[i] -= erosionPot * dt;
                     if (soil.depth[i] < 0.0f) soil.depth[i] = 0.0f; // Bedrock
                     
                     // Store Risk for Visualization
                     grid.erosion_risk[i] = std::min(1.0f, erosionPot * 1000.0f); 
                 }
            } else {
                grid.erosion_risk[i] = 0.0f;
            }
        }
    }

} // namespace landscape
