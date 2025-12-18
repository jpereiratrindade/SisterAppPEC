#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "../vegetation/vegetation_types.h"
#include "../landscape/landscape_types.h"

namespace terrain {

enum class SoilType : uint8_t {
    None = 0,
    Hidromorfico,
    BTextural,
    Argila,
    BemDes,
    Raso,
    Rocha
};

struct TerrainConfig {
    int width = 1024;
    int height = 1024;
    float scaleXZ = 1.0f; // Meters per unit
    float resolution = 1.0f; // v3.6.6: Physical meters per grid cell
    float minHeight = 0.0f;
    float maxHeight = 256.0f; // Meters
    float waterLevel = 64.0f;
    float noiseScale = 0.002f; // v3.5.0: New parameter for smoothness
    float persistence = 0.5f;  // v3.7.1: Controls roughness/jaggedness
    int octaves = 4;           // v3.5.0: New parameter for detail
    int seed = 12345;
    
    // v3.8.3: Experimental Blend Config
    // v3.8.3: Terrain Models (Refactored)
    enum class FiniteTerrainModel {
        Default,            // Standard perlin noise
        ExperimentalBlend   // Weighted frequency blend
    };

    struct BlendConfig {
        float lowFreqWeight = 1.0f;
        float midFreqWeight = 0.5f;
        float highFreqWeight = 0.25f;
        float exponent = 1.0f;
    };

    FiniteTerrainModel model = FiniteTerrainModel::Default;
    BlendConfig blendConfig;
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

    // v3.6.3: Watershed Support
    std::vector<int>& flowDirMap() { return flowDirMap_; }
    const std::vector<int>& flowDirMap() const { return flowDirMap_; }

    std::vector<int>& watershedMap() { return watershedMap_; }
    const std::vector<int>& watershedMap() const { return watershedMap_; }

    // v3.7.3: Semantic Soil Map
    std::vector<uint8_t>& soilMap() { return soilMap_; }
    const std::vector<uint8_t>& soilMap() const { return soilMap_; }
    SoilType getSoil(int x, int y) const;
    void setSoil(int x, int y, SoilType s);

    // Helpers
    // (x,z variants removed to avoid ambiguity with x,y)
    
    bool isValid(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    // v3.9.0: Vegetation Grid
    vegetation::VegetationGrid* getVegetation() { return vegGrid_.get(); }
    const vegetation::VegetationGrid* getVegetation() const { return vegGrid_.get(); }

    // v4.0: Landscape Soil Grid
    landscape::SoilGrid* getLandscapeSoil() { return landscapeSoil_.get(); }
    const landscape::SoilGrid* getLandscapeSoil() const { return landscapeSoil_.get(); }

    // v4.0: Landscape Hydro Grid
    landscape::HydroGrid* getLandscapeHydro() { return landscapeHydro_.get(); }
    const landscape::HydroGrid* getLandscapeHydro() const { return landscapeHydro_.get(); }

private:
    int width_;
    int height_;
    
    // Normalized Data [0.0 - 1.0] generally, but height can be real meters if preferred
    std::vector<float> heightMap_;
    std::vector<float> moistureMap_;
    std::vector<float> sedimentMap_; // Accumulated sediment
    std::vector<float> fluxMap_;     // Accumulated water flow (v3.6.1)
    std::vector<uint8_t> biomeMap_;  // ID of the biome
    
    // v3.6.3
    std::vector<int> flowDirMap_;    // Index of receiver cell (-1 if sink)
    std::vector<int> watershedMap_;  // ID of the drainage basin
    std::vector<uint8_t> soilMap_;   // v3.7.3: Semantic Soil ID

    // v3.9.0
    std::unique_ptr<vegetation::VegetationGrid> vegGrid_;
    
    // v4.0: Integrated Landscape (Soil Foundation)
    std::unique_ptr<landscape::SoilGrid> landscapeSoil_; // The physical soil state
    std::unique_ptr<landscape::HydroGrid> landscapeHydro_; // The hydrological state
};

} // namespace terrain
