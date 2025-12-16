#pragma once

#include <string>
#include <vector>

namespace terrain {

class TerrainMap;

struct HydrologyStats {
    // Structural
    float minElevation = 0.0f;
    float maxElevation = 0.0f;
    float avgElevation = 0.0f;

    float minSlope = 0.0f;
    float maxSlope = 0.0f;
    float avgSlope = 0.0f; // in percent or degrees? Let's use Percent for consistency with UI.

    // Functional
    float maxFlowAccumulation = 0.0f;
    float totalDischarge = 0.0f; // Sum of flux at edge or sinks? Or just max?
    float maxStreamPower = 0.0f; // E ~ A * S

    // Eco-hydrological
    float minTWI = 0.0f;
    float maxTWI = 0.0f;
    float avgTWI = 0.0f; // Topographic Wetness Index
    float saturatedAreaPct = 0.0f; // Percentage of area with TWI > 8

    // Network
    float drainageDensity = 0.0f; // Length of streams / Area
    int streamCount = 0;

    // Basins
    int basinCount = 0;
    int largestBasinArea = 0; // in cells
    float largestBasinPct = 0.0f; // % of total area

    // Per-Basin Detailed Data
    int id = 0; // 0 = Global, >0 = Basin ID
    int areaCells = 0;
    std::vector<HydrologyStats> topBasins;

    // Helper to init ranges
    void initRanges() {
        minElevation = 1e9f; maxElevation = -1e9f;
        minSlope = 1e9f; maxSlope = -1e9f;
        minTWI = 1e9f; maxTWI = -1e9f;
        maxFlowAccumulation = -1e9f;
        maxStreamPower = -1e9f;
        saturatedAreaPct = 0.0f;
        avgElevation = 0.0f;
        avgSlope = 0.0f;
        avgTWI = 0.0f;
        streamCount = 0;
        drainageDensity = 0.0f;
    }
};

class HydrologyReport {
public:
    // streamThreshold: Accumulation value to consider a cell part of a "stream" for density calc.
    // Default 100 cells? Or maybe based on map size.
    static HydrologyStats analyze(const TerrainMap& map, float resolution, float streamThreshold = 100.0f);
    
    // Generates a formatted report and saves to filepath.
    static bool generateToFile(const TerrainMap& map, float resolution, const std::string& filepath);

private:
    static float calculateSlope(const TerrainMap& map, int x, int y);
};

} // namespace terrain
