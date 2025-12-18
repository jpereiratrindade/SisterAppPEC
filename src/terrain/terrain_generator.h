#pragma once
#include "terrain_map.h"
#include "../math/noise.h"
#include <memory>

namespace terrain {

class TerrainGenerator {
public:
    TerrainGenerator(int seed = 12345);
    
    // Main processing chain
    void generateBaseTerrain(TerrainMap& map, const TerrainConfig& config);
    // v3.7.3: Semantic Soil Classification (CPU Authority)
    void classifySoil(TerrainMap& map, const TerrainConfig& config);
    // v4.0: Integrated Landscape Generation
    void generateLandscape(TerrainMap& map);
    
    // Replaced applyErosion with calculateDrainage (User Request)
    void calculateDrainage(TerrainMap& map);
    void applyErosion(TerrainMap& map, int iterations); // Kept for legacy/optional
    void generateRivers(TerrainMap& map);

private:
    math::PerlinNoise noise_;
    int seed_;
    
    struct SoilPatchConfig {
        float frequency = 1.0f;    // Controls patch size (Inverse Scale)
        float warping = 0.0f;      // Controls LSI (Domain Warp strength)
        float roughness = 0.5f;    // Controls CF (Octaves/Persistence implication)
        float stretchY = 1.0f;     // Controls RCC (Anisotropy)
    };
    
    // Helper to calculate pattern strength for a specific soil config
    float calculateSoilPattern(float x, float z, const SoilPatchConfig& config) const;
};

} // namespace terrain
