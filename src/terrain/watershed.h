#pragma once

#include <vector>
#include <cstdint>

namespace terrain {

class TerrainMap;

class Watershed {
public:
    // Delineates the watershed for a given pour point (x, y).
    // Returns a mask where 255 = inside basin, 0 = outside.
    // Also updates map.watershedMap() with a specific ID if provided > 0.
    static std::vector<uint8_t> delineate(TerrainMap& map, int startX, int startY, int basinID = 1);

    // Segments the entire terrain into basins.
    // Assigns a unique ID to map.watershedMap() for each basin draining to a sink or edge.
    // Returns the number of basins found.
    static int segmentGlobal(TerrainMap& map);
};

} // namespace terrain
