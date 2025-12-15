#include "terrain_generator.h"
#include <cmath>
#include <iostream>

namespace terrain {

TerrainGenerator::TerrainGenerator(int seed) : seed_(seed), noise_(seed) {}

void TerrainGenerator::generateBaseTerrain(TerrainMap& map, const TerrainConfig& config) {
    std::cout << "[TerrainGenerator] Generating Base Terrain (" << map.getWidth() << "x" << map.getHeight() << ")..." << std::endl;
    
    int w = map.getWidth();
    int h = map.getHeight();

    // Noise parameters
    // Scale: smaller number = larger features
    float scale = config.noiseScale;
    
    #pragma omp parallel for collapse(2)
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            // FBM (Fractal Brownian Motion)
            float nx = static_cast<float>(x) * scale;
            float nz = static_cast<float>(z) * scale;
            
            float val = 0.0f;
            float freq = 1.0f;
            float amp = 1.0f;
            float maxAmp = 0.0f;
            
            // Octaves
            for(int i=0; i<config.octaves; ++i) {
                val += noise_.noise2D(nx * freq, nz * freq) * amp;
                maxAmp += amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            
            // Normalize to [0,1]
            val /= maxAmp; 
            
            // Bias towards lower ground (val^2) to make hills stand out from plains
            val = val * 0.5f + 0.5f; // Remap -1..1 to 0..1 first if noise is signed? 
            // FastNoiseLite usually returns -1..1. Let's assume 0..1 behavior or fix it.
            // Actually noise2D might be -1..1.
            // If it's -1..1, the previous logic (val /= maxAmp) keeps it -1..1.
            // Then `val * config.maxHeight` would produce NEGATIVE heights! 
            // THIS MIGHT BE THE CAUSE OF SPIKES/HOLES! Negative values!
            
            // Fix range to 0..1
            val = (val + 1.0f) * 0.5f;
            
            // Apply exponential curve (optional, keep linear for now for smoothness)
            val = std::pow(val, 2.0f); 
            
            // Set Height (Meters)
            map.setHeight(x, z, val * config.maxHeight);
        }
    }
}

void TerrainGenerator::applyErosion(TerrainMap& map, int iterations) {
    if (iterations <= 0) return;
    std::cout << "[TerrainGenerator] Applying Hydraulic Erosion (" << iterations << " drops)..." << std::endl;

    int width = map.getWidth();
    int height = map.getHeight();
    
    // Simple hydraulic erosion parameters
    const float inertia = 0.05f;      // How much velocity is kept
    const float minSlope = 0.01f;     // Minimum slope for flow
    const float capacity = 4.0f;      // Sediment capacity factor
    const float deposition = 0.3f;    // Deposition rate
    const float erosion = 0.3f;       // Erosion rate
    const float gravity = 4.0f;
    const float evaporation = 0.02f;
    const int maxLifetime = 30;

    for (int i = 0; i < iterations; ++i) {
        // Random start position
        float x = static_cast<float>(rand() % (width - 1));
        float y = static_cast<float>(rand() % (height - 1));
        
        float dirX = 0;
        float dirY = 0;
        float speed = 1.0f;
        float water = 1.0f;
        float sediment = 0.0f;

        for (int step = 0; step < maxLifetime; ++step) {
            int nodeX = static_cast<int>(x);
            int nodeY = static_cast<int>(y);
            int cellIndex = nodeY * width + nodeX;
            
            // Calculate gradient
            float u = x - nodeX;
            float v = y - nodeY;
            
            // Get heights of 4 neighbors
            // Note: Boundary checks omitted for speed, reliant on safe margin or clamp
            if (nodeX < 0 || nodeX >= width-1 || nodeY < 0 || nodeY >= height-1) break;

            float h00 = map.heightMap()[cellIndex];
            float h10 = map.heightMap()[cellIndex + 1];
            float h01 = map.heightMap()[cellIndex + width];
            float h11 = map.heightMap()[cellIndex + width + 1];

            float gx = (h10 - h00) * (1 - v) + (h11 - h01) * v;
            float gy = (h01 - h00) * (1 - u) + (h11 - h10) * u;

            // Descent direction
            dirX = (dirX * inertia - gx * (1 - inertia));
            dirY = (dirY * inertia - gy * (1 - inertia));
            
            // Normalize
            float len = std::sqrt(dirX * dirX + dirY * dirY);
            if (len != 0) {
                dirX /= len;
                dirY /= len;
            }

            x += dirX;
            y += dirY;

            if (x < 0 || x >= width-1 || y < 0 || y >= height-1) break;

            // Height difference
            float newHeight = map.getHeight(static_cast<int>(x), static_cast<int>(y)); // simplified sampling
            float diff = (h00 + h10 + h01 + h11) * 0.25f - newHeight; // Approx old height vs new

                if (nodeX < 0 || nodeX >= width || nodeY < 0 || nodeY >= height) break;

                // Safe deposit/erode
                 float currentH = map.getHeight(nodeX, nodeY);
                 if (diff > 0) {
                        float sedimentCapacity = std::max(diff, minSlope) * speed * water * capacity;
                        if (sediment > sedimentCapacity) {
                            float amount = (sediment - sedimentCapacity) * deposition;
                            amount = std::min(amount, 0.5f); // Clamp deposition (max 0.5m/step)
                            sediment -= amount;
                            map.setHeight(nodeX, nodeY, currentH + amount); // deposit
                            map.sedimentMap()[cellIndex] += amount; 
                        } else {
                            float amount = std::min((sedimentCapacity - sediment) * erosion, diff);
                            amount = std::min(amount, 0.5f); // Clamp erosion (max 0.5m/step)
                            sediment += amount;
                            map.setHeight(nodeX, nodeY, currentH - amount); // erode
                        }
                        // Moving uphill (depression), deposit everything
                        float amount = std::min(sediment, -diff);
                         amount = std::min(amount, 0.5f); // Clamp fill
                        sediment -= amount;
                        map.setHeight(nodeX, nodeY, currentH + amount);
                        map.sedimentMap()[cellIndex] += amount;
                    }
                    
                    // v3.6.1: Accumulate Water Flux (Drainage)
                    // Accumulate `water` amount passing through this cell.
                    // To avoid accumulation explosion, we add a fraction or just 1.0 per droplet visit.
                    // Logging visits is common. Let's start with raw accumulation.
                    map.fluxMap()[cellIndex] += 1.0f; // Count visits or use `water` volume? Visits is often cleaner for "stream paths".

            speed = std::sqrt(speed * speed + diff * gravity);
            speed = std::min(speed, 10.0f); // Limit speed to prevent kinetic explosions
            water *= (1 - evaporation);
            
            if (water < 0.01f) break;
        }
    }
}

void TerrainGenerator::generateRivers(TerrainMap& map) {
    // Placeholder for river generation
    (void)map;
}

} // namespace terrain
