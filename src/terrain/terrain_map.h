#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace terrain {

struct TerrainConfig {
    int width = 1024;
    int height = 1024;
    float scaleXZ = 1.0f; // Meters per unit
    float minHeight = 0.0f;
    float maxHeight = 256.0f; // Meters
    float waterLevel = 64.0f;
    float noiseScale = 0.002f; // v3.5.0: New parameter for smoothness
    int octaves = 4;           // v3.5.0: New parameter for detail
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
    int getHeight() const { return height_; } // Keep existing getHeight() for consistency with other accessors
    
    // Helpers (renamed from original "Helpers" section, now includes direct accessors)
    float getHeight(int x, int y) const;
    void setHeight(int x, int y, float h);
    
    float getMoisture(int x, int y) const; // Changed z to y
    void setMoisture(int x, int y, float m); // Changed z to y

    float getFlux(int x, int y) const;
    void setFlux(int x, int y, float f);

    float getSediment(int x, int y) const;
    void setSediment(int x, int y, float s);
    
    // Direct buffer access for generators/renderer (reordered and consolidated)
    std::vector<float>& heightMap() { return heightMap_; }
    const std::vector<float>& heightMap() const { return heightMap_; }

    std::vector<float>& moistureMap() { return moistureMap_; }
    const std::vector<float>& moistureMap() const { return moistureMap_; }

    std::vector<float>& sedimentMap() { return sedimentMap_; }
    const std::vector<float>& sedimentMap() const { return sedimentMap_; }

    std::vector<float>& fluxMap() { return fluxMap_; }
    const std::vector<float>& fluxMap() const { return fluxMap_; }

    std::vector<uint8_t>& biomeMap() { return biomeMap_; }
    const std::vector<uint8_t>& biomeMap() const { return biomeMap_; }

    // Helpers
    // (x,z variants removed to avoid ambiguity with x,y)
    
    bool isValid(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

private:
    int width_;
    int height_;
    
    // Normalized Data [0.0 - 1.0] generally, but height can be real meters if preferred
    std::vector<float> heightMap_;
    std::vector<float> moistureMap_;
    std::vector<float> sedimentMap_; // Accumulated sediment
    std::vector<float> fluxMap_;     // Accumulated water flow (v3.6.1)
    std::vector<uint8_t> biomeMap_;  // ID of the biome
};

} // namespace terrain
