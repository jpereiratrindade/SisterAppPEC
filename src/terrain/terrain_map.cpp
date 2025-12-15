#include "terrain_map.h"
#include <algorithm>

namespace terrain {

TerrainMap::TerrainMap(int width, int height) : width_(width), height_(height) {
    resize(width, height);
}

void TerrainMap::resize(int width, int height) {
    width_ = width;
    height_ = height;
    size_t size = static_cast<size_t>(width * height);
    
    heightMap_.assign(size, 0.0f);
    moistureMap_.assign(size, 0.0f);
    sedimentMap_.assign(size, 0.0f);
    fluxMap_.assign(size, 0.0f); // v3.6.1
    biomeMap_.assign(size, 0);
}

void TerrainMap::clear() {
    std::fill(heightMap_.begin(), heightMap_.end(), 0.0f);
    std::fill(moistureMap_.begin(), moistureMap_.end(), 0.0f);
    std::fill(sedimentMap_.begin(), sedimentMap_.end(), 0.0f);
    std::fill(fluxMap_.begin(), fluxMap_.end(), 0.0f); // v3.6.1
    std::fill(biomeMap_.begin(), biomeMap_.end(), 0);
}

float TerrainMap::getHeight(int x, int z) const {
    if (!isValid(x, z)) return 0.0f;
    return heightMap_[z * width_ + x];
}

void TerrainMap::setHeight(int x, int y, float h) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        heightMap_[y * width_ + x] = h;
    }
}

float TerrainMap::getFlux(int x, int y) const {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        return fluxMap_[y * width_ + x];
    }
    return 0.0f;
}

void TerrainMap::setFlux(int x, int y, float f) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        fluxMap_[y * width_ + x] = f;
    }
}

float TerrainMap::getMoisture(int x, int z) const {
    if (!isValid(x, z)) return 0.0f;
    return moistureMap_[z * width_ + x];
}

void TerrainMap::setMoisture(int x, int z, float m) {
    if (isValid(x, z)) {
        moistureMap_[z * width_ + x] = m;
    }
}

} // namespace terrain
