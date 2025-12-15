#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace terrain {

struct TerrainConfig {
    int width = 1024;
    int height = 1024;
    float scaleXZ = 1.0f; // Meters per unit
    float maxHeight = 256.0f; // Meters
    int seed = 12345;
};

class TerrainMap {
public:
    TerrainMap(int width, int height);
    ~TerrainMap() = default;

    void resize(int width, int height);
    void clear();

    // Data Access
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    // Direct buffer access for generators/renderer
    std::vector<float>& heightMap() { return heightMap_; }
    const std::vector<float>& heightMap() const { return heightMap_; }

    std::vector<float>& moistureMap() { return moistureMap_; }
    const std::vector<float>& moistureMap() const { return moistureMap_; }

    std::vector<float>& sedimentMap() { return sedimentMap_; }
    const std::vector<float>& sedimentMap() const { return sedimentMap_; }

    std::vector<uint8_t>& biomeMap() { return biomeMap_; }
    const std::vector<uint8_t>& biomeMap() const { return biomeMap_; }

    // Helpers
    float getHeight(int x, int z) const;
    void setHeight(int x, int z, float h);
    
    float getMoisture(int x, int z) const;
    void setMoisture(int x, int z, float m);

    bool isValid(int x, int z) const {
        return x >= 0 && x < width_ && z >= 0 && z < height_;
    }

private:
    int width_;
    int height_;
    
    // Normalized Data [0.0 - 1.0] generally, but height can be real meters if preferred
    std::vector<float> heightMap_;
    std::vector<float> moistureMap_;
    std::vector<float> sedimentMap_; // Accumulated sediment
    std::vector<uint8_t> biomeMap_;  // ID of the biome
};

} // namespace terrain
