#include "soil_system.h"
#include "lithology_registry.h"
#include "../terrain/terrain_map.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>

namespace landscape {

    namespace {
        // --- Candidate Profile Definition ---
        struct CandidateProfile {
            SiBCSResult classification;
            std::string name; // Debugging
        };

        // --- Helpers ---

        // Simple slope calculation (kept for update step)
        float calculateSlope(int x, int y, const terrain::TerrainMap& terrain) {
            int w = terrain.getWidth();
            int h = terrain.getHeight();
            float H = terrain.getHeight(x, y);
            float maxDiff = 0.0f;
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
            return maxDiff; // Generic Units
        }

        // Curvature calculation (kept for update step)
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
            // Positive = Concave (Deposition), Negative = Convex (Erosion)
            return (avgNeighbor - H); 
        }

        SiBCSResult toResult(const SiBCSUserSelection& sel) {
            SiBCSResult r;
            r.order = sel.order;
            r.suborder = sel.suborder;
            r.greatGroup = sel.greatGroup;
            r.subGroup = sel.subGroup;
            r.family = sel.family;
            r.series = sel.series;
            return r;
        }

        // Helper to determine if a stored classification matches a user selection.
        bool matchesSelection(const SiBCSResult& selection, const SiBCSResult& cell) {
            auto matchLevel = [](auto selected, auto value) {
                using T = decltype(selected);
                return selected == T::kNone || selected == value;
            };

            if (selection.order == SiBCSOrder::kNone || selection.order != cell.order) return false;
            if (!matchLevel(selection.suborder, cell.suborder)) return false;
            if (!matchLevel(selection.greatGroup, cell.greatGroup)) return false;
            if (!matchLevel(selection.subGroup, cell.subGroup)) return false;
            if (!matchLevel(selection.family, cell.family)) return false;
            if (!matchLevel(selection.series, cell.series)) return false;
            return true;
        }

        SiBCSOrder orderFromSoilType(uint8_t stored) {
            switch(static_cast<SoilType>(stored)) {
                case SoilType::Latossolo: return SiBCSOrder::kLatossolo;
                case SoilType::Argissolo: return SiBCSOrder::kArgissolo;
                case SoilType::Cambissolo: return SiBCSOrder::kCambissolo;
                case SoilType::Neossolo_Litolico: return SiBCSOrder::kNeossoloLit;
                case SoilType::Neossolo_Quartzarenico: return SiBCSOrder::kNeossoloQuartz;
                case SoilType::Gleissolo: return SiBCSOrder::kGleissolo;
                case SoilType::Organossolo: return SiBCSOrder::kOrganossolo;
                default: return SiBCSOrder::kNone;
            }
        }

        SiBCSResult readCellClassification(const SoilGrid& grid, size_t idx) {
            SiBCSResult result;
            result.order = orderFromSoilType(grid.soil_type[idx]);
            result.suborder = static_cast<SiBCSSubOrder>(grid.suborder[idx]);
            result.greatGroup = static_cast<SiBCSGreatGroup>(grid.great_group[idx]);
            result.subGroup = static_cast<SiBCSSubGroup>(grid.sub_group[idx]);
            result.family = static_cast<SiBCSFamily>(grid.family[idx]);
            result.series = static_cast<SiBCSSeries>(grid.series[idx]);
            return result;
        }

        // Build list of candidates based strictly on explicit user selections (no automatic combinations)
        std::vector<CandidateProfile> generateCandidates(const SiBCSUserConfig& config) {
            std::vector<CandidateProfile> candidates;

            auto addCandidate = [&](const SiBCSResult& r) {
                if (r.order == SiBCSOrder::kNone) return;
                // Deduplicate identical selections
                auto it = std::find_if(candidates.begin(), candidates.end(), [&](const CandidateProfile& c) {
                    return c.classification.order == r.order &&
                           c.classification.suborder == r.suborder &&
                           c.classification.greatGroup == r.greatGroup &&
                           c.classification.subGroup == r.subGroup &&
                           c.classification.family == r.family &&
                           c.classification.series == r.series;
                });
                if (it == candidates.end()) {
                    CandidateProfile p;
                    p.classification = r;
                    candidates.push_back(p);
                }
            };

            // Prefer explicit selections
            for (const auto& sel : config.selections) {
                addCandidate(toResult(sel));
            }

            // Fallback: legacy allowedOrders acts as a set of explicit Order-only selections.
            if (candidates.empty()) {
                for (auto order : config.allowedOrders) {
                    SiBCSResult r;
                    r.order = order;
                    addCandidate(r);
                }
            }

            return candidates;
        }

        // Apply initialized properties based on the decided Classification
        // This is INVERSE of the old system: We set properties TO MATCH the class.
        void applyProfileEffects(SoilGrid& grid, int i_int, const CandidateProfile& profile, std::mt19937& rng) {
            size_t i = static_cast<size_t>(i_int);
            std::uniform_real_distribution<float> noise(0.9f, 1.1f);
            
            // Defaults (Cambissolo-ish)
            float depth = 1.0f;
            float clay = 0.3f;
            float sand = 0.4f;
            float om = 0.03f;
            float water = 0.2f;

            switch(profile.classification.order) {
                 case SiBCSOrder::kLatossolo:
                    depth = 2.5f; 
                    clay = 0.45f;
                    sand = 0.30f;
                    break;
                 case SiBCSOrder::kArgissolo:
                    depth = 1.5f;
                    clay = 0.35f; // B Horizon average
                    sand = 0.40f;
                    break;
                 case SiBCSOrder::kCambissolo:
                    depth = 0.8f;
                    clay = 0.25f;
                    sand = 0.45f;
                    break;
                 case SiBCSOrder::kNeossoloLit:
                    depth = 0.2f;
                    clay = 0.10f;
                    sand = 0.60f;
                    break;
                 case SiBCSOrder::kNeossoloQuartz:
                    depth = 1.8f;
                    clay = 0.05f;
                    sand = 0.90f;
                    break;
                 case SiBCSOrder::kGleissolo:
                    depth = 1.2f;
                    clay = 0.40f;
                    sand = 0.20f;
                    water = 0.9f; // Saturated
                    om = 0.08f;
                    break;
                 case SiBCSOrder::kOrganossolo:
                    depth = 0.6f;
                    om = 0.40f;
                    water = 0.8f;
                    break;
                 default: break;
            }

            // Suborder mods
            if (profile.classification.suborder == SiBCSSubOrder::kVermelho) {
                // Chemical implication: Fe2O3. Physically: often well drained.
                water *= 0.8f; 
            }
            if (profile.classification.suborder == SiBCSSubOrder::kTiomorfico) {
                om += 0.05f;
                water = 0.95f; 
            }

            // Apply to Grid with Noise
            grid.depth[i] = depth * noise(rng);
            grid.sand_fraction[i] = std::clamp(sand * noise(rng), 0.05f, 0.95f);
            grid.clay_fraction[i] = std::clamp(clay * noise(rng), 0.05f, 0.95f);
            
            // Normalize Texture
            if (grid.sand_fraction[i] + grid.clay_fraction[i] > 0.98f) {
                float s = 0.98f / (grid.sand_fraction[i] + grid.clay_fraction[i]);
                grid.sand_fraction[i] *= s;
                grid.clay_fraction[i] *= s;
            }

            grid.organic_matter[i] = om * noise(rng);
            grid.labile_carbon[i] = grid.organic_matter[i] * 0.5f;
            grid.recalcitrant_carbon[i] = grid.organic_matter[i] * 0.5f;
            
            grid.water_content_soil[i] = water;
            grid.field_capacity[i] = 0.1f + grid.clay_fraction[i] * 0.3f + grid.organic_matter[i] * 0.2f;
            grid.conductivity[i] = 0.05f + (grid.sand_fraction[i] * 0.2f);
            grid.infiltration[i] = grid.conductivity[i] * 1000.0f; // Mock conversion
            
            // Set Classification Indices
            switch(profile.classification.order) {
                case SiBCSOrder::kLatossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Latossolo); break;
                case SiBCSOrder::kArgissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Argissolo); break;
                case SiBCSOrder::kCambissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Cambissolo); break;
                case SiBCSOrder::kNeossoloLit: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Litolico); break;
                case SiBCSOrder::kNeossoloQuartz: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Neossolo_Quartzarenico); break;
                case SiBCSOrder::kGleissolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Gleissolo); break;
                case SiBCSOrder::kOrganossolo: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Organossolo); break;
                default: grid.soil_type[i] = static_cast<uint8_t>(SoilType::Undefined); break;
            }

            if (profile.classification.suborder != SiBCSSubOrder::kNone) {
                grid.suborder[i] = static_cast<uint8_t>(profile.classification.suborder);
            }
            if (profile.classification.greatGroup != SiBCSGreatGroup::kNone) {
                grid.great_group[i] = static_cast<uint8_t>(profile.classification.greatGroup);
            }
            if (profile.classification.subGroup != SiBCSSubGroup::kNone) {
                grid.sub_group[i] = static_cast<uint8_t>(profile.classification.subGroup);
            }
        }
    }

    void SoilSystem::initialize(SoilGrid& grid, int seed, const terrain::TerrainMap& terrain, SiBCSLevel targetLevel, const landscape::SiBCSUserConfig* constraints) {
        if (!grid.isValid()) return;
        (void)targetLevel;
        (void)terrain; // Terrain no longer drives classification, only physics.

        if (!constraints || !constraints->applyConstraints) {
            std::cout << "[SoilSystem] No user constraints applied. Soil initialization is idle until the user submits a domain." << std::endl;
            return; 
        }

        if (constraints->pendingChanges || !constraints->domainConfirmed) {
            std::cout << "[SoilSystem] User domain is not confirmed. Waiting for explicit Apply/Submit before simulating." << std::endl;
            return;
        }

        // 1. Generate Allowed Domain (explicit selections only)
        std::vector<CandidateProfile> candidates = generateCandidates(*constraints);
        
        if (candidates.empty()) {
            std::cout << "[SoilSystem] User constraints resulted in NO valid profiles. Check selections." << std::endl;
            return;
        }

        int w = grid.width;
        int h = grid.height;

        long long applied = 0;
        long long skippedUndefined = 0;
        long long skippedOutOfDomain = 0;

        // 2. Apply only on cells already classified by the user and inside the domain
        #pragma omp parallel for reduction(+:applied, skippedUndefined, skippedOutOfDomain) collapse(2)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i_int = y * w + x;
                size_t i = static_cast<size_t>(i_int);
                
                SiBCSResult cellClass = readCellClassification(grid, i);
                if (cellClass.order == SiBCSOrder::kNone) {
                    skippedUndefined += 1;
                    continue; // Not classified by user
                }

                int matchIdx = -1;
                for (size_t c = 0; c < candidates.size(); ++c) {
                    if (matchesSelection(candidates[c].classification, cellClass)) {
                        matchIdx = static_cast<int>(c);
                        break;
                    }
                }

                if (matchIdx < 0) {
                    grid.soil_type[i] = static_cast<uint8_t>(SoilType::Undefined);
                    skippedOutOfDomain += 1;
                    continue;
                }

                std::mt19937 localRng(static_cast<unsigned long>(seed + i_int)); 
                applyProfileEffects(grid, i_int, candidates[static_cast<size_t>(matchIdx)], localRng);
                applied += 1;
            }
        }

        std::cout << "[SoilSystem] Applied profiles to " << applied << " cells. "
                  << skippedOutOfDomain << " cells skipped (out of domain), "
                  << skippedUndefined << " cells unclassified." << std::endl;
    }

    void SoilSystem::update(SoilGrid& grid, float dt) {
        (void)grid; (void)dt;
    }

    void SoilSystem::update(SoilGrid& grid, 
                            float dt, 
                            const Climate& climate, 
                            const OrganismPressure& pressure,
                            const ParentMaterial& parent,
                            const terrain::TerrainMap& terrain,
                            int startRow, int endRow,
                            SiBCSLevel targetLevel) {
        
        (void)targetLevel;

        int w = grid.width;
        int h = grid.height;
        if (startRow < 0) startRow = 0;
        if (endRow < 0 || endRow > h) endRow = h;

        PedogenesisService pedogenesis;

        #pragma omp parallel for collapse(2)
        for (int y = startRow; y < endRow; ++y) {
            for (int x = 0; x < w; ++x) {
                int i_int = y * w + x;
                size_t i = static_cast<size_t>(i_int);
                
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

                // 2. Evolve (SCORPAN Processes)
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

                grid.organic_matter[i] = grid.labile_carbon[i] + grid.recalcitrant_carbon[i];
                grid.infiltration[i] = grid.conductivity[i] * 1000.0f; 

                // NO Classification Step.
            }
        }
    }

} // namespace landscape
