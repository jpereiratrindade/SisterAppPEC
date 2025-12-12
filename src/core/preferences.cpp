#include "preferences.h"
#include <sstream>

namespace core {

Preferences::Preferences() {
    // Default values matched with voxel_terrain.h
}

void Preferences::load() {
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        std::cout << "[Preferences] No preferences file found, using defaults." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key;
        ss >> key;
        
        if (key == "slope_flat_max_pct") {
            ss >> currentSlopeConfig_.flatMaxPct;
        } else if (key == "slope_gentle_max_pct") {
            ss >> currentSlopeConfig_.gentleMaxPct;
        } else if (key == "slope_steep_max_pct") {
            ss >> currentSlopeConfig_.steepMaxPct;
        }
    }
    std::cout << "[Preferences] Loaded settings from " << filePath_ << std::endl;
}

void Preferences::save() {
    std::ofstream file(filePath_);
    if (!file.is_open()) {
        std::cerr << "[Preferences] Failed to open " << filePath_ << " for writing." << std::endl;
        return;
    }

    file << "slope_flat_max_pct " << currentSlopeConfig_.flatMaxPct << "\n";
    file << "slope_gentle_max_pct " << currentSlopeConfig_.gentleMaxPct << "\n";
    file << "slope_steep_max_pct " << currentSlopeConfig_.steepMaxPct << "\n";

    std::cout << "[Preferences] Saved settings to " << filePath_ << std::endl;
}

} // namespace core
