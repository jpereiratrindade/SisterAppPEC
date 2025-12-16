#include "terrain_generator.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <vector>

namespace terrain {

TerrainGenerator::TerrainGenerator(int seed) : noise_(seed), seed_(seed) {
}

// 1. SEED FIX: Use config.seed
void TerrainGenerator::generateBaseTerrain(TerrainMap& map, const TerrainConfig& config) {
    if (config.seed != 0) {
        seed_ = config.seed;
       // Seed the generator
    // noise_.SetSeed(seed_); // FastNoiseLite has SetSeed, but math::PerlinNoise might not.
    // Checking header: PerlinNoise(int) is constructor. No SetSeed.
    // So we reconstruct it.
    noise_ = math::PerlinNoise(seed_);
    }

    std::cout << "[TerrainGenerator] Generating Base Terrain (" << map.getWidth() << "x" << map.getHeight() << "), Seed: " << seed_ << "..." << std::endl;
    
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

// 2. D8 FIX: Use Slope (Drop/Distance)
void TerrainGenerator::calculateDrainage(TerrainMap& map) {
    std::cout << "[TerrainGenerator] Calculating Drainage (D8 w/ Physical Slope)..." << std::endl;
    int w = map.getWidth();
    int h = map.getHeight();
    int size = w * h;

    // Reset Flux
    std::fill(map.fluxMap().begin(), map.fluxMap().end(), 1.0f);

    // 1. Calculate Downstream Indices (Steepest Descent)
    std::vector<int> downstream(size, -1);
    
    // Sort indices by height (high to low)
    std::vector<int> sortedIndices(size);
    for(int i=0; i<size; ++i) sortedIndices[i] = i;
    
    const auto& heightMap = map.heightMap();
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b){
        return heightMap[a] > heightMap[b];
    });

    // We need resolution for correct physical slope, but if uniform X/Z, relative comparison holds if we just correct for diagonals.
    // However, to be pedantically correct per request:
    // Slope = (H_curr - H_neighbor) / (dist_cells * resolution)
    // Resolution cancels out for comparison, but DISTANCE FACTOR (1 vs 1.414) is critical.
    
    // 2. Determine Receiver for each cell
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            float currentH = heightMap[idx];
            float maxSlope = 0.0f; // Track Slope, not Drop
            int receiver = -1;

            // Check 8 neighbors
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        float drop = currentH - heightMap[ny * w + nx];
                        if (drop > 0) {
                            // Distance factor: 1.0 for cardinal, 1.414 for diagonal
                            float distFactor = (dx == 0 || dy == 0) ? 1.0f : 1.41421356f;
                            float slope = drop / distFactor; 
                            
                            if (slope > maxSlope) {
                                 maxSlope = slope;
                                 receiver = ny * w + nx;
                            }
                        }
                    }
                }
            }
            downstream[idx] = receiver;
            // v3.6.3: Store permanently in map
            map.flowDirMap()[idx] = receiver;
        }
    }

    // 3. Accumulate Flow
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

    // --- Landscape Ecology Patch Configuration ---
    // Mapping LSI/CF/RCC to Noise Parameters
    struct SoilPatchConfig {
        float frequency;    // Patch Size (Inverse)
        float warping;      // LSI (Distortion)
        float roughness;    // CF (Complexity/Octaves)
        float stretchY;     // RCC (Anisotropy)
    };

    // Constant configs for soil types
    static const std::map<SoilType, SoilPatchConfig> soilConfigs = {
        // Solo Raso: LSI High (5434), CF Mod (2.49), RCC 0.66
        {SoilType::Raso,          {1.5f, 25.0f, 0.8f, 1.2f}}, 
        
        // Bem Desenvolvido: LSI Low (2508), CF Low (2.36), RCC High (0.68 - Most Circular)
        {SoilType::BemDes,        {0.8f, 2.0f,  0.2f, 1.0f}}, 
        
        // Hidromorfico: LSI Mod (3272), CF Low (2.27), RCC 0.65
        {SoilType::Hidromorfico,  {1.2f, 8.0f,  0.3f, 1.5f}}, 
        
        // Argila Expansiva: LSI Low (1827), CF High (2.84), RCC 0.64 (Alongado/Irregular)
        {SoilType::Argila,        {2.0f, 5.0f,  0.9f, 0.6f}}, 
        
        // B-Textural: Avg values
        {SoilType::BTextural,     {1.0f, 10.0f, 0.5f, 1.0f}},
        
        {SoilType::Rocha,         {1.0f, 0.0f,  0.5f, 1.0f}} // Fallback
    };

    // Helper Lambda for Pattern Strength
    // calculateSoilPattern is thread-safe (const method using const noise_)
    
    #pragma omp parallel for collapse(2)
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            float hL = map.getHeight(std::max(0, x-1), z);
            float hR = map.getHeight(std::min(w-1, x+1), z);
            float hD = map.getHeight(x, std::max(0, z-1));
            float hU = map.getHeight(x, std::min(h-1, z+1));
            
            float dz_dx = (hR - hL) / (2.0f * config.resolution);
            float dz_dz = (hU - hD) / (2.0f * config.resolution);
            
            float localSlope = std::sqrt(dz_dx*dz_dx + dz_dz*dz_dz) * 100.0f;

            // Candidate Soils based on Slope Class (Catena)
            std::vector<SoilType> candidates;

            if (localSlope < 3.0f) {
                candidates = {SoilType::Hidromorfico, SoilType::BTextural, SoilType::Argila};
            } else if (localSlope < 8.0f) {
                candidates = {SoilType::BTextural, SoilType::BemDes, SoilType::Argila};
            } else if (localSlope < 20.0f) {
                candidates = {SoilType::BTextural, SoilType::Argila};
            } else if (localSlope < 45.0f) {
                candidates = {SoilType::BTextural, SoilType::Raso};
            } else if (localSlope < 75.0f) {
                candidates = {SoilType::Raso};
            } else {
                candidates = {SoilType::Rocha};
            }

            // Competition: Select candidate with highest pattern strength
            SoilType bestSoil = SoilType::Rocha; // Default
            float maxStrength = -1e9f;

            for (auto type : candidates) {
                // Inline call or use lambda? Lambda capture might be tricky with OMP if not careful.
                // Just calling direct helper.
                const auto& cfg = soilConfigs.at(type);
                float strength = calculateSoilPattern(static_cast<float>(x), static_cast<float>(z), {cfg.frequency, cfg.warping, cfg.roughness, cfg.stretchY});
                
                if (strength > maxStrength) {
                    maxStrength = strength;
                    bestSoil = type;
                }
            }
            map.setSoil(x, z, bestSoil);
        }
    }
}

float TerrainGenerator::calculateSoilPattern(float x, float z, const SoilPatchConfig& cfg) const {
    // 1. Anisotropy (RCC): Scale coordinates
    float nx = x * 0.01f * cfg.frequency;
    float nz = z * 0.01f * cfg.frequency * cfg.stretchY;

    // 2. Domain Warp (LSI): Offset coordinates using noise
    if (cfg.warping > 0.0f) {
        float qx = noise_.noise2D(nx + 5.2f, nz + 1.3f);
        float qz = noise_.noise2D(nx + 1.3f, nz + 5.2f);
        nx += qx * cfg.warping * 0.01f; 
        nz += qz * cfg.warping * 0.01f;
    }

    // 3. Complexity (CF): Octave Noise
    int octaves = 1 + static_cast<int>(cfg.roughness * 4.0f); // 1 to 5
    float persistence = 0.3f + (cfg.roughness * 0.4f);        // 0.3 to 0.7

    // Shift origin per config/soil to decorrelate? 
    // We rely on diff freq/warp. Ideally check soil type index offset. 
    // For now, rely on different params.
    
    return noise_.octaveNoise(nx, nz, octaves, persistence);
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
