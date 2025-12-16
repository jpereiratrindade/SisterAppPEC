#include "hydrology_report.h"
#include "terrain_map.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

namespace terrain {

float HydrologyReport::calculateSlope(const TerrainMap& map, int x, int y) {
    float h0 = map.getHeight(x, y);
    float maxSlope = 0.0f;
     float scale = 1.0f; // Assuming 1.0 scale if not exposed, but usually passed or 1.
    // Actually TerrainConfig has scaleXZ. TerrainMap doesn't expose config directly, 
    // but usually we can assume 1.0 or need to pass it. 
    // Looking at terrain_generator.cpp, scaleXZ is in config. 
    // For now, assume unit distance = 1.0f for grid cells unless we find otherwise.
    // If map has physical height, we should use that.
    
    // Check 8 neighbors
    const int w = map.getWidth();
    const int h = map.getHeight();
    
    // Directions: N, NE, E, SE, S, SW, W, NW
    const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    const float distMult[] = {1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f};

    for (int i = 0; i < 8; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];

        if (map.isValid(nx, ny)) {
            float hN = map.getHeight(nx, ny);
            float drop = h0 - hN;
            if (drop > 0) {
                float slope = drop / distMult[i]; // rise / run
                if (slope > maxSlope) {
                    maxSlope = slope;
                }
            }
        }
    }
    return maxSlope;
}

HydrologyStats HydrologyReport::analyze(const TerrainMap& map, float resolution, float streamThreshold) {
    if (resolution <= 0.0f) resolution = 1.0f;
    float cellArea = resolution * resolution;

    HydrologyStats globalStats;
    globalStats.initRanges();
    globalStats.id = 0;
    globalStats.areaCells = map.getWidth() * map.getHeight();

    int w = map.getWidth();
    int h = map.getHeight();
    int count = w * h;

    // ... (rest is largely same logic but updated math)
    
    // Basin map/vectors omitted for brevity in replacement chunk, 
    // will assume we replace whole body or critical loops.
    // Actually, let's just replace the critical math parts if possible, 
    // but the logic is interspersed. 
    // Re-writing the function with physical units.
    
    // ...
    // Let's do a targeted replace of the loop logic if possible, 
    // but `analyze` signature changed.
    
    // We will replace the whole function to get it right.
    std::map<int, HydrologyStats> basinStatsMap;
    // ...
    
    struct Accumulator {
        double sumElev = 0.0;
        double sumSlope = 0.0;
        double sumTWI = 0.0;
        int twiCount = 0;
        float streamLength = 0.0f; // Changed from Cells to Length (meters)
    };
    std::map<int, Accumulator> basinAccMap;
    
    // Directions and Distances for Slope/Length
    const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    const float distMult[] = {1.0f, 1.41421356f, 1.0f, 1.41421356f, 1.0f, 1.41421356f, 1.0f, 1.41421356f};

    double g_sumElev = 0.0;
    double g_sumSlope = 0.0;
    double g_sumTWI = 0.0;
    int g_twiCount = 0;
    float g_streamLength = 0.0f; // Physical Length in Meters

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float elev = map.getHeight(x, y);
            float fluxCells = map.getFlux(x, y);
            int idx = y * w + x;
            int bid = !map.watershedMap().empty() ? map.watershedMap()[idx] : 0;

            // --- PHYSICAL PARAMETERS ---
            // 1. Slope: Max Drop / (Dist * Res)
            float maxSlope = 0.0f;
            for (int i = 0; i < 8; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                if (map.isValid(nx, ny)) {
                    float drop = elev - map.getHeight(nx, ny);
                    if (drop > 0) {
                        float dist = distMult[i] * resolution;
                        float s = drop / dist;
                        if (s > maxSlope) maxSlope = s;
                    }
                }
            }
            // If local flat/pit, slope=0

            // 2. Specific Catchment Area (a)
            // a = CatchmentArea / ContourWidth
            // CatchmentArea = FluxCells * CellArea = FluxCells * Res * Res
            // ContourWidth ~= Resolution (approx)
            // So a = FluxCells * Res
            float specificArea = fluxCells * resolution;
            
            // 3. TWI = ln(a / tanB)
            float tanB = std::max(maxSlope, 0.001f); // Avoid div by zero, 0.1% slope min
            float twi = std::log(specificArea / tanB);

            // 4. Stream Channel
            // Threshold is usually on FluxCells.
            bool isStream = (fluxCells >= streamThreshold);
            float localStreamLen = 0.0f;
            if (isStream) {
                // If it's a stream, what length does it contribute?
                // For D8, it flows to 1 receiver. We can take half distance to receiver + half from upstream?
                // Simplification: Each stream cell adds 'Resolution' length?
                // Or better: Distance to receiver?
                // Let's check receiver.
                int receiver = map.flowDirMap().empty() ? -1 : map.flowDirMap()[idx];
                if (receiver != -1) {
                    int rx = receiver % w;
                    int ry = receiver / w;
                    int dX = std::abs(rx - x);
                    int dY = std::abs(ry - y);
                    float distFactor = (dX+dY == 2) ? 1.41421356f : 1.0f; // 2 means diagonal (1+1)
                    localStreamLen = distFactor * resolution;
                } else {
                    localStreamLen = resolution; // Outline/sink
                }
            }

            // --- GLOBAL STATS ---
            // Elevation
            if (elev < globalStats.minElevation) globalStats.minElevation = elev;
            if (elev > globalStats.maxElevation) globalStats.maxElevation = elev;
            g_sumElev += elev;

            // Slope (tan beta)
            if (maxSlope < globalStats.minSlope) globalStats.minSlope = maxSlope;
            if (maxSlope > globalStats.maxSlope) globalStats.maxSlope = maxSlope;
            g_sumSlope += maxSlope;

            // Flow Accumulation (Physical Area m2)
            float flowArea = fluxCells * cellArea;
            if (flowArea > globalStats.maxFlowAccumulation) globalStats.maxFlowAccumulation = flowArea;
            
            // Stream Power (SPI = A * S) -> Specific Area * Slope ? Or Total Area?
            // Usually SPI = a * tanB.
            float spi = specificArea * maxSlope; 
            if (spi > globalStats.maxStreamPower) globalStats.maxStreamPower = spi;

            // TWI
            if (twi < globalStats.minTWI) globalStats.minTWI = twi;
            if (twi > globalStats.maxTWI) globalStats.maxTWI = twi;
            g_sumTWI += twi;
            g_twiCount++;
            if (twi > 8.0f) globalStats.saturatedAreaPct += 1.0f;

            // Network
            g_streamLength += localStreamLen;


            // --- BASIN STATS ---
            if (bid > 0) {
                if (basinStatsMap.find(bid) == basinStatsMap.end()) {
                    basinStatsMap[bid].initRanges();
                    basinStatsMap[bid].id = bid;
                    basinStatsMap[bid].areaCells = 0;
                }
                
                HydrologyStats& bStats = basinStatsMap[bid];
                Accumulator& bAcc = basinAccMap[bid];

                bStats.areaCells++;

                // Elev
                if (elev < bStats.minElevation) bStats.minElevation = elev;
                if (elev > bStats.maxElevation) bStats.maxElevation = elev;
                bAcc.sumElev += elev;

                // Slope
                if (maxSlope < bStats.minSlope) bStats.minSlope = maxSlope;
                if (maxSlope > bStats.maxSlope) bStats.maxSlope = maxSlope;
                bAcc.sumSlope += maxSlope;

                // Flow
                if (flowArea > bStats.maxFlowAccumulation) bStats.maxFlowAccumulation = flowArea;
                if (spi > bStats.maxStreamPower) bStats.maxStreamPower = spi;

                // TWI
                if (twi < bStats.minTWI) bStats.minTWI = twi;
                if (twi > bStats.maxTWI) bStats.maxTWI = twi;
                bAcc.sumTWI += twi;
                bAcc.twiCount++;
                if (twi > 8.0f) bStats.saturatedAreaPct += 1.0f;

                // Network
                bAcc.streamLength += localStreamLen;
            }
        }
    }

    // --- FINALIZE GLOBAL ---
    globalStats.avgElevation = static_cast<float>(g_sumElev / count);
    globalStats.avgSlope = static_cast<float>(g_sumSlope / count);
    if (g_twiCount > 0) globalStats.avgTWI = static_cast<float>(g_sumTWI / g_twiCount);
    
    // Percent of Physical Area
    // (Count > 8 / Total Count) * 100
    globalStats.saturatedAreaPct = (globalStats.saturatedAreaPct / static_cast<float>(count)) * 100.0f;
    
    // Drainage Density = Total Channel Length / Total Area
    float totalAreaM2 = count * cellArea;
    if (totalAreaM2 > 0) {
        globalStats.drainageDensity = g_streamLength / totalAreaM2; // m/m2 = 1/m
    }
    
    // Stream Count (Number of segments/cells, legacy metric)
    globalStats.streamCount = static_cast<int>(g_streamLength / resolution); // approx

    // --- FINALIZE BASINS ---
    globalStats.basinCount = 0;
    globalStats.largestBasinArea = 0;
    globalStats.largestBasinPct = 0.0f;

    std::vector<HydrologyStats> allBasins;
    for (auto& kv : basinStatsMap) {
        int bid = kv.first;
        HydrologyStats& bs = kv.second;
        const Accumulator& acc = basinAccMap[bid];

        if (bs.areaCells > 0) {
            bs.avgElevation = static_cast<float>(acc.sumElev / bs.areaCells);
            bs.avgSlope = static_cast<float>(acc.sumSlope / bs.areaCells);
            if (acc.twiCount > 0) bs.avgTWI = static_cast<float>(acc.sumTWI / acc.twiCount);
            
            bs.saturatedAreaPct = (bs.saturatedAreaPct / static_cast<float>(bs.areaCells)) * 100.0f;
            
            float basinAreaM2 = bs.areaCells * cellArea;
            if (basinAreaM2 > 0) {
                bs.drainageDensity = acc.streamLength / basinAreaM2;
            }
            bs.streamCount = static_cast<int>(acc.streamLength / resolution);
            
            allBasins.push_back(bs);
        }
    }

    if (!allBasins.empty()) {
        globalStats.basinCount = static_cast<int>(allBasins.size());
        
        std::sort(allBasins.begin(), allBasins.end(), [](const HydrologyStats& a, const HydrologyStats& b){
            return a.areaCells > b.areaCells;
        });

        globalStats.largestBasinArea = allBasins[0].areaCells;
        globalStats.largestBasinPct = (float(allBasins[0].areaCells) / float(count)) * 100.0f;
        
        int keep = std::min((int)allBasins.size(), 3);
        for(int i=0; i<keep; ++i) {
            globalStats.topBasins.push_back(allBasins[i]);
        }
    }

    return globalStats;
}

bool HydrologyReport::generateToFile(const TerrainMap& map, float resolution, const std::string& filepath) {
    // Analyze first
    HydrologyStats stats = analyze(map, resolution, 100.0f); // default threshold of 100 cells for stream initiation
    
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "Declividade (m/m):\n";
    out << "  - Metodo:              Steepest Descent (Max Drop / Distance)\n";
    out << "  - Media:               " << stats.avgSlope << " (" << (stats.avgSlope*100.0f) << "%)\n";
    out << "  - Maxima:              " << stats.maxSlope << " (" << (stats.maxSlope*100.0f) << "%)\n\n";

    out << "3. PARAMETROS FUNCIONAIS (HIDROLOGIA)\n";
    out << "-----------------------------------------------------------------\n";
    out << "Area de Contribuicao (Fluxo Acumulado):\n";
    out << "  - Maximo:              " << stats.maxFlowAccumulation << " m2\n\n";

    out << "Potencia do Fluxo (Stream Power Index ~ A_spec * S):\n";
    out << "  - Maximo:              " << stats.maxStreamPower << "\n";
    out << "  - Indicativo de potencial geomorfologico: Regioes com alto SPI sao suscetiveis.\n\n";

    out << "4. PARAMETROS ECO-HIDROLOGICOS\n";
    out << "-----------------------------------------------------------------\n";
    out << "Indice Topografico de Umidade (TWI = ln(a / tanB)):\n";
    out << "  - Minimo:              " << stats.minTWI << " (Zonas secas/divisores)\n";
    out << "  - Maximo:              " << stats.maxTWI << " (Zonas saturadas)\n";
    out << "  - Medio:               " << stats.avgTWI << "\n";
    out << "  - Area com TWI > 8:    " << stats.saturatedAreaPct << " %\n\n";

    out << "5. REDE DE DRENAGEM\n";
    out << "-----------------------------------------------------------------\n";
    out << "Threshold de Canalizacao: Fluxo (Cells) > 100\n";
    out << "Densidade de Drenagem:\n";
    out << "  - Densidade:           " << std::scientific << stats.drainageDensity << " m/m2 (m-1)\n";
    if (stats.drainageDensity > 0.0f) {
        out << "  - Equivalente:         " << std::fixed << std::setprecision(2) << (stats.drainageDensity * 1000.0f) << " Km de rios por Km2\n";
    }
    out << "  - Extension Total:     " << (stats.streamCount * resolution) << " m (approx)\n\n";

    if (stats.basinCount > 0) {
        out << "6. ESTATISTICAS DE BACIAS (Watershed Segmentation)\n";
        out << "-----------------------------------------------------------------\n";
        out << std::fixed << std::setprecision(0);
        out << "Total de Bacias Identificadas: " << stats.basinCount << "\n";
        out << "Maior Bacia (Area):            " << (stats.largestBasinArea * resolution * resolution) << " m2\n";
        out << std::fixed << std::setprecision(2);
        out << "Dominancia da Maior Bacia:     " << stats.largestBasinPct << " % da area total\n\n";

        out << "7. ANALISE DETALHADA: PRINCIPAIS BACIAS\n";
        out << "-----------------------------------------------------------------\n";
        
        int bIndex = 1;
        for (const auto& basin : stats.topBasins) {
            float bAreaM2 = basin.areaCells * resolution * resolution;
            out << "7." << bIndex << ". BACIA ID " << basin.id << " (Area: " << bAreaM2 << " m2)\n";
            out << "   - Elevação (Min/Med/Max):     " << basin.minElevation << " / " << basin.avgElevation << " / " << basin.maxElevation << "\n";
            out << "   - Declividade Média:          " << basin.avgSlope << " (" << (basin.avgSlope * 100.0f) << "%)\n";
            out << "   - TWI Médio:                  " << basin.avgTWI << "\n";
            out << "   - Saturação (TWI>8):          " << basin.saturatedAreaPct << " %\n";
            out << "   - Densidade Drenagem:         " << std::scientific << basin.drainageDensity << std::fixed << " m-1\n";
            out << "   - Stream Power Max:           " << basin.maxStreamPower << "\n";
            out << "\n";
            bIndex++;
        }
    }

    out << "=================================================================\n";
    out << "Fim do Relatorio.\n";

    out.close();
    return true;
}

} // namespace terrain
