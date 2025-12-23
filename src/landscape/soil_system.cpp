#include "soil_system.h"
#include "lithology_registry.h"
#include "../terrain/terrain_map.h"
#include "../math/noise.h" // v4.5.4
#include <cmath>
#include <algorithm>
#include <random>

namespace landscape {

    // Helper: Simple slope calculation
    float calculateSlope(int x, int y, const terrain::TerrainMap& terrain) {
        int w = terrain.getWidth();
        int h = terrain.getHeight();
        float H = terrain.getHeight(x, y);
        
        float maxDiff = 0.0f;
        // Check 4 neighbors
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};
        
        for(int k=0; k<4; ++k) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if(nx >=0 && nx < w && ny >= 0 && ny < h) {
                float diff = std::abs(terrain.getHeight(nx, ny) - H);
                if(diff > maxDiff) maxDiff = diff;
            }
        }
        return maxDiff; // Approximation
    }

    // Helper: Curvature calculation (simple Laplacian)
    float calculateCurvature(int x, int y, const terrain::TerrainMap& terrain) {
        int w = terrain.getWidth();
        int h = terrain.getHeight();
        float H = terrain.getHeight(x, y);
        
        float sumH = 0.0f;
        int count = 0;
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};
        
        for(int k=0; k<4; ++k) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if(nx >=0 && nx < w && ny >= 0 && ny < h) {
                sumH += terrain.getHeight(nx, ny);
                count++;
            }
        }
        
        if (count == 0) return 0.0f;
        float avgNeighbor = sumH / static_cast<float>(count);
        return (avgNeighbor - H); // Positive = Concave (Deposition), Negative = Convex (Erosion)
    }

    void SoilSystem::initialize(SoilGrid& grid, int seed, const terrain::TerrainMap& terrain, SiBCSLevel targetLevel) {
        if (!grid.isValid()) return;
        
        int w = grid.width;
        int h = grid.height;
        std::mt19937 gen(static_cast<unsigned int>(seed));
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        // v4.5.4: Use Spatial Noise for Soil Variation
        math::PerlinNoise noise(static_cast<unsigned int>(seed) + 54321u);
        math::PerlinNoise noiseGeo(static_cast<unsigned int>(seed) + 98765u);
        
        const auto& lithologyRegistry = LithologyRegistry::instance();

        // Instantiate Domain Services
        SoilPhysicsService physicsService;
        SiBCSClassifier sibcsClassifier;

        #pragma omp parallel for collapse(2)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = y * w + x;
                float slope = calculateSlope(x, y, terrain);
                float curvature = calculateCurvature(x, y, terrain);
                float elevation = terrain.getHeight(x, y);
                
                // Get Parent Material Properties
                const auto& rockDef = lithologyRegistry.get(grid.lithology_id[i]);

                // Initial Pedogenesis Guess based on Rock + Slope
                float weatheringPotential = rockDef.weathering_rate * (1.0f - std::min(1.0f, slope * 0.1f));
                
                // Depth logic (Continuous)
                float maxDepth = (0.5f + weatheringPotential) * 2.5f; // Potential up to ~3m
                float depthFactor = std::exp(-slope * 10.0f); 
                float depthNoise = 0.8f + dis(gen) * 0.4f; // +/- 20%
                grid.depth[i] = maxDepth * depthFactor * depthNoise;
                if (grid.depth[i] < 0.1f) grid.depth[i] = 0.1f;

                // 1. Base Texture Generation (SCORPAN)
                float sx = static_cast<float>(x) * 0.005f;
                float sz = static_cast<float>(y) * 0.005f;
                float texNoise = noiseGeo.octaveNoise(sx, sz, 3, 0.5f); 
                
                float sandBase = rockDef.sand_bias + (texNoise - 0.5f) * 0.6f; 
                float clayBase = rockDef.clay_bias - (texNoise - 0.5f) * 0.5f; 
                
                grid.sand_fraction[i] = std::clamp(sandBase, 0.05f, 0.95f);
                grid.clay_fraction[i] = std::clamp(clayBase, 0.05f, 0.95f);
                
                // Normalize
                float sum = grid.sand_fraction[i] + grid.clay_fraction[i];
                if(sum > 0.95f) {
                    float s = 0.95f/sum;
                    grid.sand_fraction[i] *= s;
                    grid.clay_fraction[i] *= s;
                }

                // 2. Apply Topography (Mechanistic Step)
                SoilMineralState mineralState;
                mineralState.sand_fraction = grid.sand_fraction[i];
                mineralState.clay_fraction = grid.clay_fraction[i];
                mineralState.depth = grid.depth[i]; 

                Relief relief;
                relief.slope = slope;
                relief.curvature = curvature;
                relief.elevation = elevation;

                physicsService.applyTopographyToTexture(mineralState, relief);

                grid.sand_fraction[i] = static_cast<float>(mineralState.sand_fraction);
                grid.clay_fraction[i] = static_cast<float>(mineralState.clay_fraction);

                // 3. Organic Matter & Hydrology
                float ox = static_cast<float>(x) * 0.01f;
                float oz = static_cast<float>(y) * 0.01f;
                float bioNoise = noise.octaveNoise(ox, oz, 2, 0.5f);
                float topoBonus = (curvature > 0) ? curvature * 10.0f : 0.0f; 

                grid.labile_carbon[i] = 0.01f + bioNoise * 0.04f + std::min(0.05f, topoBonus); 
                grid.recalcitrant_carbon[i] = 0.01f;
                grid.dead_biomass[i] = 0.0f;
                grid.organic_matter[i] = grid.labile_carbon[i] + grid.recalcitrant_carbon[i];
                
                grid.water_content_soil[i] = 0.1f + std::max(0.0f, curvature * 50.0f);
                grid.field_capacity[i] = 0.2f + grid.clay_fraction[i] * 0.2f; 
                grid.infiltration[i] = 50.0f * (1.0f + grid.sand_fraction[i] - grid.clay_fraction[i]);

                // 4. SiBCS Classification (Unified Multi-Level)
                SoilState currentState;
                currentState.mineral = mineralState;
                currentState.organic.labile_carbon = grid.labile_carbon[i];
                currentState.organic.recalcitrant_carbon = grid.recalcitrant_carbon[i];
                currentState.hydric.water_content = grid.water_content_soil[i];
                currentState.hydric.field_capacity = grid.field_capacity[i];

                SiBCSResult result = sibcsClassifier.classify(currentState, relief, targetLevel);

                // legacy Mapping: SiBCS Order -> SoilType (for compatibility)
                switch(result.order) {
                    case SiBCSOrder::kLatossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Latossolo); break;
                    case SiBCSOrder::kArgissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Argissolo); break;
                    case SiBCSOrder::kCambissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Cambissolo); break;
                    case SiBCSOrder::kNeossoloLit: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Litolico); break;
                    case SiBCSOrder::kNeossoloQuartz: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Quartzarenico); break;
                    case SiBCSOrder::kGleissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Gleissolo); break;
                    case SiBCSOrder::kOrganossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Organossolo); break;
                    default: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Cambissolo); break;
                }

                // Map results to grid
                grid.suborder[i] = static_cast<uint8_t>(result.suborder);
                grid.great_group[i] = static_cast<uint8_t>(result.greatGroup);
                grid.sub_group[i] = static_cast<uint8_t>(result.subGroup);
                grid.family[i] = static_cast<uint8_t>(result.family);
                grid.series[i] = static_cast<uint8_t>(result.series);
                
                // Legacy Fallback for SCORPAN mode safety (e.g. renderers expecting basic Lithic logic)
                if (grid.depth[i] < 0.2f) grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Litolico);
            }
        }
    }

    void SoilSystem::update(SoilGrid& /*grid*/, float /*dt*/) {
        // Legacy/Empty update
    }

    void SoilSystem::update(SoilGrid& grid, 
                            float dt, 
                            const Climate& climate, 
                            const OrganismPressure& pressure,
                            const ParentMaterial& parent,
                            const terrain::TerrainMap& terrain,
                            int startRow, int endRow,
                            SiBCSLevel targetLevel) {
        
        int w = grid.width;
        int h = grid.height;
        
        // Time Slicing Defaults
        if (startRow < 0) startRow = 0;
        if (endRow < 0 || endRow > h) endRow = h;
        
        PedogenesisService pedogenesis;
        SiBCSClassifier sibcsClassifier; 

        #pragma omp parallel for collapse(2)
        for (int y = startRow; y < endRow; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = y * w + x;
                
                // 1. Construct State objects
                ParentMaterial mat = parent; 
                
                Relief relief;
                relief.elevation = terrain.getHeight(x, y);
                relief.slope = calculateSlope(x, y, terrain);
                relief.curvature = calculateCurvature(x, y, terrain);

                SoilState current;
                current.mineral.depth = grid.depth[i];
                current.mineral.sand_fraction = grid.sand_fraction[i];
                current.mineral.clay_fraction = grid.clay_fraction[i];
                
                current.organic.labile_carbon = grid.labile_carbon[i];
                current.organic.recalcitrant_carbon = grid.recalcitrant_carbon[i];
                current.organic.dead_biomass = grid.dead_biomass[i];

                current.hydric.water_content = grid.water_content_soil[i];
                current.hydric.field_capacity = grid.field_capacity[i];
                current.hydric.conductivity = grid.conductivity[i];

                // 2. Evolve
                SoilState next = pedogenesis.evolve(current, mat, relief, climate, pressure, dt);

                // 3. Write Back
                grid.depth[i] = static_cast<float>(next.mineral.depth);
                grid.sand_fraction[i] = static_cast<float>(next.mineral.sand_fraction);
                grid.clay_fraction[i] = static_cast<float>(next.mineral.clay_fraction);
                
                grid.labile_carbon[i] = static_cast<float>(next.organic.labile_carbon);
                grid.recalcitrant_carbon[i] = static_cast<float>(next.organic.recalcitrant_carbon);
                grid.dead_biomass[i] = static_cast<float>(next.organic.dead_biomass);
                
                grid.water_content_soil[i] = static_cast<float>(next.hydric.water_content);
                grid.field_capacity[i] = static_cast<float>(next.hydric.field_capacity);
                grid.conductivity[i] = static_cast<float>(next.hydric.conductivity);

                // Update Derived / Simplified properties
                grid.organic_matter[i] = grid.labile_carbon[i] + grid.recalcitrant_carbon[i];
                grid.infiltration[i] = grid.conductivity[i] * 1000.0f; 

                // 4. Update Type Classification (Unified)
                SoilState classifierState;
                classifierState.mineral.depth = grid.depth[i];
                classifierState.mineral.sand_fraction = grid.sand_fraction[i];
                classifierState.mineral.clay_fraction = grid.clay_fraction[i];
                classifierState.organic.labile_carbon = grid.labile_carbon[i];
                classifierState.organic.recalcitrant_carbon = grid.recalcitrant_carbon[i];
                classifierState.hydric.water_content = grid.water_content_soil[i];
                classifierState.hydric.field_capacity = grid.field_capacity[i];

                SiBCSResult result = sibcsClassifier.classify(classifierState, relief, targetLevel);

                // Legacy Mapping
                 switch(result.order) {
                    case SiBCSOrder::kLatossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Latossolo); break;
                    case SiBCSOrder::kArgissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Argissolo); break;
                    case SiBCSOrder::kCambissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Cambissolo); break;
                    case SiBCSOrder::kNeossoloLit: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Litolico); break;
                    case SiBCSOrder::kNeossoloQuartz: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Quartzarenico); break;
                    case SiBCSOrder::kGleissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Gleissolo); break;
                    case SiBCSOrder::kOrganossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Organossolo); break;
                    default: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Cambissolo); break;
                }

                grid.suborder[i] = static_cast<uint8_t>(result.suborder);
                grid.great_group[i] = static_cast<uint8_t>(result.greatGroup);
                grid.sub_group[i] = static_cast<uint8_t>(result.subGroup);
                grid.family[i] = static_cast<uint8_t>(result.family);
                grid.series[i] = static_cast<uint8_t>(result.series);
            }
        }
    }

} // namespace landscape
