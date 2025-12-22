#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>

namespace landscape {

    // Definition of a Rock Type (Parent Material)
    struct LithologyDef {
        std::string name;
        
        // Physical Parameters (0.0 - 1.0)
        float weathering_rate;  // Speed of soil formation (0=Granite, 1=Marl)
        float base_fertility;   // Intrinsic nutrient content (0=Acid, 1=Eutrophic)
        
        // Texture Generation Bias
        float sand_bias;        // Tendency to generate Sand (0-1)
        float clay_bias;        // Tendency to generate Clay (0-1)
        
        // Visual
        float r, g, b;          // Color for "Geology View"
    };

    class LithologyRegistry {
    public:
        static LithologyRegistry& instance() {
            static LithologyRegistry inst;
            return inst;
        }

        void registerLithology(uint8_t id, const LithologyDef& def) {
            defs_[id] = def;
        }

        const LithologyDef& get(uint8_t id) const {
            auto it = defs_.find(id);
            if (it != defs_.end()) return it->second;
            return defaultDef_;
        }

        const std::map<uint8_t, LithologyDef>& getAll() const {
            return defs_;
        }
        
        // Helper to populate default rocks
        void initDefaults() {
            if (!defs_.empty()) return;
            
            // ID 0: Generic Sediment
            registerLithology(0, {"Generic Sediment", 0.5f, 0.5f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f});
            
            // ID 1: Basalto (Terra Roxa parent)
            registerLithology(1, {"Basalto (Vulc)", 0.8f, 1.0f, 0.1f, 0.9f, 0.4f, 0.1f, 0.1f});
            
            // ID 2: Granito (Shield parent)
            registerLithology(2, {"Granito (Ign)", 0.2f, 0.3f, 0.8f, 0.2f, 0.8f, 0.6f, 0.6f});
            
            // ID 3: Arenito (Sediment parent)
            registerLithology(3, {"Arenito (Sed)", 0.6f, 0.1f, 0.95f, 0.05f, 0.9f, 0.8f, 0.4f});
            
            std::cout << "[Geology] Initialized Default Lithologies." << std::endl;
        }

    private:
        LithologyRegistry() {
            // Safe default
            defaultDef_ = {"Unknown", 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f};
        }
        
        std::map<uint8_t, LithologyDef> defs_;
        LithologyDef defaultDef_;
    };

} // namespace landscape
