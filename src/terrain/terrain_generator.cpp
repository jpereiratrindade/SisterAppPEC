#include "terrain_generator.h"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace terrain {

TerrainGenerator::TerrainGenerator(int seed) : noise_(seed), seed_(seed) {
}

void TerrainGenerator::generateBaseTerrain(TerrainMap& map, const TerrainConfig& config) {
    std::cout << "[TerrainGenerator] Generating Base Terrain (" << map.getWidth() << "x" << map.getHeight() << ")..." << std::endl;
    
    int w = map.getWidth();
    int h = map.getHeight();

    // Noise parameters
    float scale = config.noiseScale;
    
    #pragma omp parallel for collapse(2)
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            // FBM
            // v3.6.6: Use Physical Coordinates (x * resolution) for Noise Sampling
            // This ensures terrain shape/roughness depends on physical distance (meters), not grid index.
            float nx = (static_cast<float>(x) * config.resolution) * scale;
            float nz = (static_cast<float>(z) * config.resolution) * scale;
            
            float val = 0.0f;
            float freq = 1.0f;
            float amp = 1.0f;
            float maxAmp = 0.0f;
            
            for(int i=0; i<config.octaves; ++i) {
                val += noise_.noise2D(nx * freq, nz * freq) * amp;
                maxAmp += amp;
                amp *= config.persistence; // v3.7.1
                freq *= 2.0f;
            }
            
            val /= maxAmp; 
            
            // Map -1..1 to 0..1
            val = (val + 1.0f) * 0.5f;
            
            // Apply curve
            val = std::pow(val, 2.0f); 
            
            // Set Height
            map.setHeight(x, z, val * config.maxHeight);
        }
    }
}

void TerrainGenerator::calculateDrainage(TerrainMap& map) {
    std::cout << "[TerrainGenerator] Calculating Drainage (D8 Algorithm)..." << std::endl;
    int w = map.getWidth();
    int h = map.getHeight();
    int size = w * h;

    // Reset Flux
    std::fill(map.fluxMap().begin(), map.fluxMap().end(), 1.0f);

    // 1. Calculate Downstream Indices (Steepest Descent)
    std::vector<int> downstream(size, -1);
    
    // Sort cells by height (highest to lowest) to accumulate flow
    // Storing indices
    std::vector<int> sortedIndices(size);
    for(int i=0; i<size; ++i) sortedIndices[i] = i;
    
    // We need access to heightMap for sorting.
    const auto& heightMap = map.heightMap();
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b){
        return heightMap[a] > heightMap[b];
    });

    // 2. Determine Receiver for each cell
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            float currentH = heightMap[idx];
            float maxDrop = 0.0f;
            int receiver = -1;

            // Check 8 neighbors
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        float drop = currentH - heightMap[ny * w + nx];
                        if (drop > 0 && drop > maxDrop) {
                             maxDrop = drop;
                             receiver = ny * w + nx;
                        }
                    }
                }
            }
            downstream[idx] = receiver;
            // v3.6.3: Store permanently in map
            map.flowDirMap()[idx] = receiver;
        }
    }

    // 3. Accumulate Flow (Cascade from high to low)
    // Since we iterate from high to low, the upstream nodes are processed before downstream.
    for (int idx : sortedIndices) {
        int receiver = map.flowDirMap()[idx];
        if (receiver != -1) {
            map.fluxMap()[receiver] += map.fluxMap()[idx];
        }
    }
    
    std::cout << "[TerrainGenerator] Drainage Calculation Complete." << std::endl;
}




void TerrainGenerator::classifySoil(TerrainMap& map, const TerrainConfig& config) {
    int w = map.getWidth();
    int h = map.getHeight();


    #pragma omp parallel for collapse(2)
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            // Calculate Slope
            // Central Difference
            float hL = map.getHeight(std::max(0, x-1), z);
            float hR = map.getHeight(std::min(w-1, x+1), z);
            float hD = map.getHeight(x, std::max(0, z-1));
            float hU = map.getHeight(x, std::min(h-1, z+1));
            
            float dz_dx = (hR - hL) / (2.0f * config.resolution);
            float dz_dz = (hU - hD) / (2.0f * config.resolution);
            
            float slopePct = std::sqrt(dz_dx*dz_dx + dz_dz*dz_dz) * 100.0f;
            
            // Stochastic Factor: Coherent Value Noise
            // Shader: float rnd = noise(fragWorldPos.xz * 0.1);
            // We use the noise_ member. It returns -1..1 usually (FastNoiseLite/Simplex).
            // But basic noise2D returns -1..1. We need 0..1.
            
            float wx = static_cast<float>(x) * config.resolution;
            float wz = static_cast<float>(z) * config.resolution;
            
            float rndVal = noise_.noise2D(wx * 0.1f, wz * 0.1f);
            float rnd = (rndVal + 1.0f) * 0.5f; 
            
            SoilType type = SoilType::None;

            // Soil Selection Logic (Matches Shader v3.7.2)
            // Note: Application.cpp has "Allowed" flags. Here we generate the "potential" soil type.
            // The shader filtered by "Allowed" flags.
            // Requirement: "CPU defines soil". This implies the "Allowed" flags should optionally filter here 
            // OR we generate the "Best Natural" soil and let the visualizer filter?
            // User Request: "A GPU nao decide o solo... CPU define".
            // Implementation: We will assign the definitive natural soil type here.
            // If the user wants to toggle visibility of certain soils, the Shader/Mesh update can handle masking,
            // OR we just assume all are active for the "Natural" state.
            // Let's implement the full natural distribution logic.
            
            if (slopePct < 3.0f) {
                // Plano (0-3%): Hidro (33%), BText (33%), Argila (33%)
                // Shader had weighted selection based on count of enabled.
                // We assume all enabled for base classification.
                if (rnd < 0.333f) type = SoilType::Hidromorfico;
                else if (rnd < 0.666f) type = SoilType::BTextural;
                else type = SoilType::Argila;
            } 
            else if (slopePct < 8.0f) {
                // Suave (3-8%): BText, BemDes, Argila
                if (rnd < 0.333f) type = SoilType::BTextural;
                else if (rnd < 0.666f) type = SoilType::BemDes;
                else type = SoilType::Argila;
            }
            else if (slopePct < 20.0f) {
                // Ondulado (8-20%): BText, Argila
                if (rnd < 0.5f) type = SoilType::BTextural;
                else type = SoilType::Argila;
            }
            else if (slopePct < 45.0f) {
                // Forte (20-45%): BText, Raso
                if (rnd < 0.5f) type = SoilType::BTextural;
                else type = SoilType::Raso;
            }
            else if (slopePct < 75.0f) {
                // Escarpado: Raso
                type = SoilType::Raso;
            }
            else {
                // Extreme: Rocha
                type = SoilType::Rocha;
            }
            
            map.setSoil(x, z, type);
        }
    }
    


}

// Keeping empty implementation for now to satisfy link, or basic one.
void TerrainGenerator::applyErosion(TerrainMap& map, int /*iterations*/) {
    // Optional: Can add simple erosion based on the calculated flux later.
    // Stream Power Law: Erosion = K * Flux^m * Slope^n
    // For now, user wants just Drainage.
    (void)map;
}

void TerrainGenerator::generateRivers(TerrainMap& map) {
    (void)map;
}

} // namespace terrain
