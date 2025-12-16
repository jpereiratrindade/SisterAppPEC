#pragma once

#include "terrain_map.h"
#include "watershed.h"
#include <map>
#include <vector>
#include <string>

namespace terrain {

struct ClassMetrics {
    SoilType type;
    int pixelCount = 0;
    int edgeCount = 0; // Number of edges shared with different soil or boundary
    
    // Physical Metrics
    double area_m2 = 0.0;
    double perimeter_m = 0.0;
    
    // Indices (Farina)
    double LSI = 0.0; // Landscape Shape Index
    double CF = 0.0;  // Complexity of Form
    double RCC = 0.0; // Relative Circularity Coefficient
};

class LandscapeMetricCalculator {
public:
    // Global Analysis
    static std::map<SoilType, ClassMetrics> analyzeGlobal(const TerrainMap& map, float resolution);

    // Basin Analysis
    // Basin Analysis
    // Uses the watershedMap stored within the TerrainMap
    static std::map<int, std::map<SoilType, ClassMetrics>> analyzeByBasin(const TerrainMap& map, float resolution);

    // Helper to format report string
    static std::string formatReport(const std::map<SoilType, ClassMetrics>& metrics, const std::string& title);
};

} // namespace terrain
