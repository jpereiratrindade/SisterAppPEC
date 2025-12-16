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
    
    // Replaced applyErosion with calculateDrainage (User Request)
    void calculateDrainage(TerrainMap& map);
    void applyErosion(TerrainMap& map, int iterations); // Kept for legacy/optional
    void generateRivers(TerrainMap& map);

private:
    math::PerlinNoise noise_;
    int seed_;
};

} // namespace terrain
