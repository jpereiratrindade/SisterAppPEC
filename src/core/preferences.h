#pragma once

#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include "../terrain/slope_config.h"

namespace core {

/**
 * @brief Manages persistence of application settings (JSON-style simple parser/writer)
 */
class Preferences {
public:
    static Preferences& instance() {
        static Preferences instance;
        return instance;
    }

    void load();
    void save();

    // Accessors
    graphics::SlopeConfig getSlopeConfig() const { return currentSlopeConfig_; }
    void setSlopeConfig(const graphics::SlopeConfig& config) { currentSlopeConfig_ = config; }

    std::string getFilePath() const { return filePath_; }

private:
    Preferences();
    ~Preferences() = default;

    std::string filePath_ = "prefs.json";
    graphics::SlopeConfig currentSlopeConfig_;
};

} // namespace core
