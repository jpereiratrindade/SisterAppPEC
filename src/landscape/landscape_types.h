#pragma once

#include <vector>
#include <cstdint>
#include <vector>

namespace landscape {

    // Soil Types (Simplified Classification)
    enum class SoilType : uint8_t {
        Sandy_Loam = 0, // High infiltration, low retention
        Clay_Loam = 1,  // Low infiltration, high retention
        Silt_Loam = 2,  // Balanced
        Rocky = 3,      // Very low depth, high runoff
        Peat = 4        // High organic, high retention
    };

    /**
     * @brief Structure-of-Arrays (SoA) for Soil State.
     * Represents the physical and biological potential of the substrate.
     * 
     * Complies with DDD v1.2:
     * - depth: Dynamic state subject to erosion (Service: HydroSystem)
     * - infiltration: Base physical property (Service: HydroSystem)
     * - compaction: Process state (Service: SoilSystem)
     * - propagule_bank: Biological memory (Service: GrowthService)
     */
    // v4.4.0: Geology Layer
    using LithologyID = uint8_t;

    struct SoilGrid {
        int width = 0;
        int height = 0;

        // Physical Properties
        std::vector<float> depth;          // [meters] Effective soil depth. 0 = Bedrock.
        std::vector<float> infiltration;   // [mm/h] Base K_sat (Saturated Hydraulic Conductivity)
        std::vector<float> compaction;     // [0.0 - 1.0] 0 = Porous, 1 = Sealed (Reduces infiltration)
        std::vector<float> organic_matter; // [0.0 - 1.0] Enhances structure/water holding

        // Biological Memory (Resilience Factor)
        std::vector<float> propagule_bank; // [0.0 - 1.0] Potential for regeneration (Seeds/Buds)

        // Classification & Geology
        std::vector<uint8_t> soil_type;    // Maps to SoilType
        std::vector<LithologyID> lithology_id; // v4.4.0: ID of Parent Material

        void resize(int w, int h) {
            width = w;
            height = h;
            size_t size = static_cast<size_t>(w * h);

            depth.assign(size, 1.0f);           // Default 1m depth
            infiltration.assign(size, 50.0f);   // Default 50mm/h (Loam)
            compaction.assign(size, 0.0f);      // No compaction
            organic_matter.assign(size, 0.5f);  // Moderate organic matter
            propagule_bank.assign(size, 1.0f);  // Full regenerative potential
            soil_type.assign(size, static_cast<uint8_t>(SoilType::Silt_Loam));
            lithology_id.assign(size, 0);       // Default Lithology (0 = Generic)
        }

        size_t getSize() const { return depth.size(); }

        bool isValid() const {
            return !depth.empty() && 
                   depth.size() == lithology_id.size() &&
                   width * height == static_cast<int>(depth.size());
        }
    };

    /**
     * @brief Hydrological State (Water & Flow).
     * Connects Climate (Rain) -> Topography (Flow) -> Soil/Veg (Infiltration).
     */
    struct HydroGrid {
        int width = 0;
        int height = 0;

        // State Variables
        std::vector<float> water_depth;      // [m] Surface water depth (Runoff)
        std::vector<float> flow_flux;        // [m^3/s?? or Unitless Accumulation] Accumulated Flow
        std::vector<float> erosion_risk;     // [0-1] Calculated Stream Power / Shear Stress

        // Topological Cache for Fast Flow Routing
        // We compute flow directions once (static topography) and use this order to propagate water.
        std::vector<int> receiver_index;     // [Cell Index] Where water goes (-1 = Sink)
        std::vector<int> sort_order;         // [Cell Index] Order from High to Low elevation
        std::vector<float> slope;            // [Tan theta] Pre-calculated physical slope

        void resize(int w, int h) {
            width = w;
            height = h;
            size_t size = static_cast<size_t>(w * h);

            water_depth.assign(size, 0.0f);
            flow_flux.assign(size, 0.0f);
            erosion_risk.assign(size, 0.0f);
            receiver_index.assign(size, -1);
            sort_order.resize(size);
            slope.assign(size, 0.0f);
        }

        bool isValid() const {
            return !water_depth.empty() && width * height == static_cast<int>(water_depth.size());
        }
    };

} // namespace landscape
