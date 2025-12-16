#include "landscape_metrics.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace terrain {

std::map<SoilType, ClassMetrics> LandscapeMetricCalculator::analyzeGlobal(const TerrainMap& map, float resolution) {
    std::map<SoilType, ClassMetrics> results;
    
    // Initialize for all types
    std::vector<SoilType> types = {
        SoilType::Raso, SoilType::BemDes, SoilType::Hidromorfico, 
        SoilType::Argila, SoilType::BTextural, SoilType::Rocha
    };
    for(auto t : types) results[t].type = t;

    int w = map.getWidth();
    int h = map.getHeight();
    const auto& soilMap = map.soilMap();

    // Single Pass: Count Pixels and Edges
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            int idx = z * w + x;
            SoilType current = static_cast<SoilType>(soilMap[idx]);
            if (current == SoilType::None) continue;

            results[current].pixelCount++;

            // Check 4 neighbors for edges
            // Edge exists if neighbor is different OR out of bounds
            
            // North
            if (z + 1 < h) {
                if (static_cast<SoilType>(soilMap[idx + w]) != current) results[current].edgeCount++;
            } else {
                results[current].edgeCount++; // Boundary
            }

            // South
            if (z - 1 >= 0) {
                if (static_cast<SoilType>(soilMap[idx - w]) != current) results[current].edgeCount++;
            } else {
                results[current].edgeCount++;
            }

            // East
            if (x + 1 < w) {
                if (static_cast<SoilType>(soilMap[idx + 1]) != current) results[current].edgeCount++;
            } else {
                results[current].edgeCount++;
            }

            // West
            if (x - 1 >= 0) {
                if (static_cast<SoilType>(soilMap[idx - 1]) != current) results[current].edgeCount++;
            } else {
                results[current].edgeCount++;
            }
        }
    }

    // Calculate Indices
    for (auto& [type, m] : results) {
        if (m.pixelCount == 0) continue;

        m.area_m2 = m.pixelCount * (resolution * resolution);
        m.perimeter_m = m.edgeCount * resolution;

        // LSI: P / 2*sqrt(pi*A)
        m.LSI = m.perimeter_m / (2.0 * std::sqrt(M_PI * m.area_m2));
        
        // CF: P / A (Context: Farina sometimes uses Edge Density? But formula in DDD says P/A)
        // Note: P/A usually has units m^-1. DDD formula: CF = P / A.
        m.CF = m.perimeter_m / m.area_m2;

        // RCC: 4*pi*A / P^2
        if (m.perimeter_m > 0) {
            m.RCC = (4.0 * M_PI * m.area_m2) / (m.perimeter_m * m.perimeter_m);
        } else {
            m.RCC = 0.0;
        }
    }

    return results;
}

std::map<int, std::map<SoilType, ClassMetrics>> LandscapeMetricCalculator::analyzeByBasin(const TerrainMap& map, float resolution) {
    std::map<int, std::map<SoilType, ClassMetrics>> basinResults;
    
    int w = map.getWidth();
    int h = map.getHeight();
    const auto& soilMap = map.soilMap();
    const auto& watershedMap = map.watershedMap();

    // Pass
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            int idx = z * w + x;
            int basinId = watershedMap[idx];
            
            // Only analyze main basins (ID > 0)
            if (basinId <= 0) continue;

            SoilType current = static_cast<SoilType>(soilMap[idx]);
            if (current == SoilType::None) continue;

            ClassMetrics& m = basinResults[basinId][current];
            m.type = current; // Ensure type is set
            m.pixelCount++;
            
            // Logic: Evaluate neighbors. If neighbor is out of basin, it's an edge.
            auto checkNeighbor = [&](int nx, int nz) {
                if (nx < 0 || nx >= w || nz < 0 || nz >= h) return true; // Boundary
                int nidx = nz * w + nx;
                if (watershedMap[nidx] != basinId) return true; // Basin Boundary
                if (static_cast<SoilType>(soilMap[nidx]) != current) return true;      // Soil Boundary
                return false;
            };

            if (checkNeighbor(x+1, z)) m.edgeCount++;
            if (checkNeighbor(x-1, z)) m.edgeCount++;
            if (checkNeighbor(x, z+1)) m.edgeCount++;
            if (checkNeighbor(x, z-1)) m.edgeCount++;
        }
    }

    // Calc Indices
    for (auto& [bid, metricsMap] : basinResults) {
        for (auto& [type, m] : metricsMap) {
            m.area_m2 = m.pixelCount * (resolution * resolution);
            m.perimeter_m = m.edgeCount * resolution;
            
            if (m.area_m2 > 0) {
                m.LSI = m.perimeter_m / (2.0 * std::sqrt(M_PI * m.area_m2));
                m.CF = m.perimeter_m / m.area_m2;
                if (m.perimeter_m > 0) m.RCC = (4.0 * M_PI * m.area_m2) / (m.perimeter_m * m.perimeter_m);
            }
        }
    }

    return basinResults;
}

std::string LandscapeMetricCalculator::formatReport(const std::map<SoilType, ClassMetrics>& metrics, const std::string& title) {
    std::stringstream ss;
    ss << title << "\n";
    ss << "Soil Type | Area (m2) | Perimeter (m) | LSI | CF | RCC\n";
    ss << "----------|-----------|---------------|-----|----|-----\n";
    
    // Helper names
    auto getName = [](SoilType t) -> std::string {
        switch(t) {
            case SoilType::Raso: return "Raso";
            case SoilType::BemDes: return "BemDes";
            case SoilType::Hidromorfico: return "Hidro";
            case SoilType::Argila: return "Argila";
            case SoilType::BTextural: return "BText";
            case SoilType::Rocha: return "Rocha";
            default: return "None";
        }
    };

    for (const auto& [type, m] : metrics) {
        if (m.pixelCount == 0) continue;
        ss << std::left << std::setw(10) << getName(type) << " | "
           << std::fixed << std::setprecision(1) << std::setw(9) << m.area_m2 << " | "
           << std::setw(13) << m.perimeter_m << " | "
           << std::setprecision(3) << std::setw(3) << m.LSI << " | "
           << std::setw(2) << m.CF << " | "
           << std::setw(3) << m.RCC << "\n";
    }
    ss << "\n";
    return ss.str();
}

} // namespace terrain
