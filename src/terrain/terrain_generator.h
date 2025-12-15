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
    void applyErosion(TerrainMap& map, int iterations = 50000);
    void generateRivers(TerrainMap& map);

private:
    math::PerlinNoise noise_;
    int seed_;
};

} // namespace terrain
