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

HydrologyStats HydrologyReport::analyze(const TerrainMap& map, float streamThreshold) {
    HydrologyStats globalStats;
    globalStats.initRanges();
    globalStats.id = 0;
    globalStats.areaCells = map.getWidth() * map.getHeight();

    int w = map.getWidth();
    int h = map.getHeight();
    int count = w * h;

    // Basin tracking
    const auto& watershed = map.watershedMap();
    bool hasBasins = !watershed.empty();
    
    // Map of Basin ID -> Stats
    // Using a vector is faster if we know max ID, but map is safer if IDs are sparse.
    // However, segmentGlobal usually produces contiguous IDs from 1.
    // Let's use a vector, resizing if needed or pre-scanning max ID.
    // For safety/simplicity in single pass, let's just use a Map or large vector if known.
    // Since we don't know Max ID easily without scan, let's do a map for now or assume < 65536.
    // But wait, we iterate pixels. Let's lazily add to a map to avoid scan.
    // Note: allocating map nodes per pixel might be slow?
    // Optimization: Pre-scan max ID or use `watershedCounter` from TerrainMap if available.
    // Let's assume max 1000 basins for now? No, use std::map<int, HydrologyStats>.
    std::map<int, HydrologyStats> basinStatsMap;

    // Accumulators for Global (since struct holds final avgs, we need doubles for sums)
    double g_sumElev = 0.0;
    double g_sumSlope = 0.0;
    double g_sumTWI = 0.0;
    int g_twiCount = 0;
    float g_streamCells = 0.0f;

    // Accumulators for Basins (we need to store running sums)
    // We can't easily store running sums in HydrologyStats (floats).
    // Let's create a helper struct or just "abuse" the float fields for sums temporarily?
    // No, precision loss.
    // Let's iterate twice? No.
    // Let's make `basinStatsMap` store the running sums in the `avg` fields (as doubles? no).
    // Let's make a local struct for summation.
    struct Accumulator {
        double sumElev = 0.0;
        double sumSlope = 0.0;
        double sumTWI = 0.0;
        int twiCount = 0;
        float streamCells = 0.0f;
    };
    std::map<int, Accumulator> basinAccMap;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float elev = map.getHeight(x, y);
            float flux = map.getFlux(x, y);
            int idx = y * w + x;
            int bid = hasBasins ? watershed[idx] : 0;

            // --- GLOBAL STATS ---
            // Elevation
            if (elev < globalStats.minElevation) globalStats.minElevation = elev;
            if (elev > globalStats.maxElevation) globalStats.maxElevation = elev;
            g_sumElev += elev;

            // Slope
            float slope = calculateSlope(map, x, y);
            if (slope < globalStats.minSlope) globalStats.minSlope = slope;
            if (slope > globalStats.maxSlope) globalStats.maxSlope = slope;
            g_sumSlope += slope;

            // Flow
            if (flux > globalStats.maxFlowAccumulation) globalStats.maxFlowAccumulation = flux;
            float spi = flux * slope;
            if (spi > globalStats.maxStreamPower) globalStats.maxStreamPower = spi;

            // TWI
            float tanB = std::max(slope, 0.0001f);
            float twi = std::log(flux / tanB);
            if (twi < globalStats.minTWI) globalStats.minTWI = twi;
            if (twi > globalStats.maxTWI) globalStats.maxTWI = twi;
            g_sumTWI += twi;
            g_twiCount++;
            if (twi > 8.0f) globalStats.saturatedAreaPct += 1.0f;

            // Network
            if (flux >= streamThreshold) g_streamCells += 1.0f;

            // --- BASIN STATS ---
            if (bid > 0) {
                // Initialize if new
                if (basinStatsMap.find(bid) == basinStatsMap.end()) {
                    basinStatsMap[bid].initRanges();
                    basinStatsMap[bid].id = bid;
                    basinStatsMap[bid].areaCells = 0;
                }
                
                HydrologyStats& bStats = basinStatsMap[bid];
                Accumulator& bAcc = basinAccMap[bid]; // will create default

                bStats.areaCells++;

                // Elev
                if (elev < bStats.minElevation) bStats.minElevation = elev;
                if (elev > bStats.maxElevation) bStats.maxElevation = elev;
                bAcc.sumElev += elev;

                // Slope
                if (slope < bStats.minSlope) bStats.minSlope = slope;
                if (slope > bStats.maxSlope) bStats.maxSlope = slope;
                bAcc.sumSlope += slope;

                // Flow
                if (flux > bStats.maxFlowAccumulation) bStats.maxFlowAccumulation = flux;
                if (spi > bStats.maxStreamPower) bStats.maxStreamPower = spi;

                // TWI
                if (twi < bStats.minTWI) bStats.minTWI = twi;
                if (twi > bStats.maxTWI) bStats.maxTWI = twi;
                bAcc.sumTWI += twi;
                bAcc.twiCount++;
                if (twi > 8.0f) bStats.saturatedAreaPct += 1.0f;

                // Network
                if (flux >= streamThreshold) bAcc.streamCells += 1.0f;
            }
        }
    }

    // --- FINALIZE GLOBAL ---
    globalStats.avgElevation = static_cast<float>(g_sumElev / count);
    globalStats.avgSlope = static_cast<float>(g_sumSlope / count);
    globalStats.avgTWI = static_cast<float>(g_sumTWI / g_twiCount);
    globalStats.saturatedAreaPct = (globalStats.saturatedAreaPct / static_cast<float>(count)) * 100.0f;
    globalStats.drainageDensity = g_streamCells / static_cast<float>(count);
    globalStats.streamCount = static_cast<int>(g_streamCells);

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
            bs.avgTWI = static_cast<float>(acc.sumTWI / acc.twiCount);
            bs.saturatedAreaPct = (bs.saturatedAreaPct / static_cast<float>(bs.areaCells)) * 100.0f;
            bs.drainageDensity = acc.streamCells / static_cast<float>(bs.areaCells);
            bs.streamCount = static_cast<int>(acc.streamCells);
            
            allBasins.push_back(bs);
        }
    }

    if (!allBasins.empty()) {
        // Collect Global Basin Summary Stats
        globalStats.basinCount = static_cast<int>(allBasins.size());
        
        // Sort by area descending
        std::sort(allBasins.begin(), allBasins.end(), [](const HydrologyStats& a, const HydrologyStats& b){
            return a.areaCells > b.areaCells;
        });

        globalStats.largestBasinArea = allBasins[0].areaCells;
        globalStats.largestBasinPct = (float(allBasins[0].areaCells) / float(count)) * 100.0f;
        
        // Keep Top 3
        int keep = std::min((int)allBasins.size(), 3);
        for(int i=0; i<keep; ++i) {
            globalStats.topBasins.push_back(allBasins[i]);
        }
    }

    return globalStats;
}

bool HydrologyReport::generateToFile(const TerrainMap& map, const std::string& filepath) {
    // Analyze first
    // Threshold: Flux > 50 makes a stream? Adjust scaling if needed. 
    // Usually log(Flux) is visualized. Let's pick 100 as default base.
    HydrologyStats stats = analyze(map, 100.0f);

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "=================================================================\n";
    out << "                RELATORIO DE ANALISE HIDROLOGICA                 \n";
    out << "       Analise Estrutural e Funcional de Modelo Idealizado       \n";
    out << "=================================================================\n\n";

    out << "NOTA TEORICA:\n";
    out << "Este relatorio apresenta uma analise estrutural e funcional de um\n";
    out << "modelo hidrologico digital. Os resultados devem ser interpretados\n";
    out << "como propriedades emergentes da topografia e das regras de fluxo,\n";
    out << "respondendo a como o relevo organiza gradientes e fluxos.\n";
    out << "Resultados dependem das resolucoes e parametros adotados no modelo.\n";
    out << "NAO DEVE ser utilizado como previsao de vazao absoluta ou erosao real.\n\n";

    out << "1. INFORMACOES GERAIS\n";
    out << "-----------------------------------------------------------------\n";
    out << "Dimensoes da Grade:      " << map.getWidth() << " x " << map.getHeight() << "\n";
    out << "Resolucao Espacial:      1.0 u.m. (simbolica)\n";
    out << "Total de Celulas:        " << (map.getWidth() * map.getHeight()) << "\n";
    out << "Unidade de Medida:       Unidades de Modelo (u.m.)\n\n";

    out << "2. PARAMETROS ESTRUTURAIS (TOPOGRAFIA)\n";
    out << "-----------------------------------------------------------------\n";
    out << std::fixed << std::setprecision(2);
    out << "Eleuacao (u.m.):\n";
    out << "  - Minima:              " << stats.minElevation << "\n";
    out << "  - Maxima:              " << stats.maxElevation << "\n";
    out << "  - Media:               " << stats.avgElevation << "\n";
    out << "  - Amplitude:           " << (stats.maxElevation - stats.minElevation) << "\n\n";

    out << "Declividade (tan beta):\n";
    out << "  - Metodo:              Steepest Descent (Maximo declive local 8-vizinhos)\n";
    out << "  - Media:               " << stats.avgSlope << " (" << (stats.avgSlope*100.0f) << "%)\n";
    out << "  - Maxima:              " << stats.maxSlope << " (" << (stats.maxSlope*100.0f) << "%)\n\n";

    out << "3. PARAMETROS FUNCIONAIS (HIDROLOGIA)\n";
    out << "-----------------------------------------------------------------\n";
    out << "Fluxo Acumulado (Area de Contribuicao proxy):\n";
    out << "  - Maximo:              " << stats.maxFlowAccumulation << "\n\n";

    out << "Potencia do Fluxo (Stream Power Index ~ A * S):\n";
    out << "  - Maximo:              " << stats.maxStreamPower << "\n";
    out << "  - Indicativo de potencial geomorfologico: Regioes com alto SPI sao suscetiveis.\n\n";

    out << "4. PARAMETROS ECO-HIDROLOGICOS\n";
    out << "-----------------------------------------------------------------\n";
    out << "Indice Topografico de Umidade (TWI = ln(A / tanB)):\n";
    out << "  - Minimo:              " << stats.minTWI << " (Zonas secas/divisores)\n";
    out << "  - Maximo:              " << stats.maxTWI << " (Zonas saturadas)\n";
    out << "  - Medio:               " << stats.avgTWI << "\n";
    out << "  - Area com TWI > 8:    " << stats.saturatedAreaPct << " %\n\n";

    out << "5. REDE DE DRENAGEM\n";
    out << "-----------------------------------------------------------------\n";
    out << "Threshold de Canalizacao: Fluxo > 100\n";
    out << "Densidade de Drenagem:\n";
    out << "  - Densidade:           " << std::scientific << stats.drainageDensity << " streams/pixel\n";
    if (stats.drainageDensity > 0.0f) {
        out << "  - Equivalente:         1 canal a cada ~" << std::fixed << std::setprecision(0) << (1.0f / stats.drainageDensity) << " pixels\n";
    }
    out << "  - Vol. Streams:        " << stats.streamCount << " celulas\n\n";

    if (stats.basinCount > 0) {
        out << "6. ESTATISTICAS DE BACIAS (Watershed Segmentation)\n";
        out << "-----------------------------------------------------------------\n";
        out << std::fixed << std::setprecision(0);
        out << "Total de Bacias Identificadas: " << stats.basinCount << "\n";
        out << "Maior Bacia (Area):            " << stats.largestBasinArea << " celulas\n";
        out << std::fixed << std::setprecision(2);
        out << "Dominancia da Maior Bacia:     " << stats.largestBasinPct << " % da area total\n\n";

        out << "7. ANALISE DETALHADA: PRINCIPAIS BACIAS\n";
        out << "-----------------------------------------------------------------\n";
        
        int bIndex = 1;
        for (const auto& basin : stats.topBasins) {
            out << "7." << bIndex << ". BACIA ID " << basin.id << " (Area: " << basin.areaCells << " px)\n";
            out << "   - Elevação (Min/Med/Max):     " << basin.minElevation << " / " << basin.avgElevation << " / " << basin.maxElevation << "\n";
            out << "   - Declividade Média:          " << basin.avgSlope << " (" << (basin.avgSlope * 100.0f) << "%)\n";
            out << "   - TWI Médio:                  " << basin.avgTWI << "\n";
            out << "   - Saturação (TWI>8):          " << basin.saturatedAreaPct << " %\n";
            out << "   - Densidade Drenagem:         " << std::scientific << basin.drainageDensity << std::fixed << "\n";
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
