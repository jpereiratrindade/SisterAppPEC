#pragma once

namespace graphics {

/**
 * @brief Configuration for slope classification thresholds (percentages).
 * v3.4.0: Slope Analysis Tools
 */
struct SlopeConfig {
    float flatMaxPct = 3.0f;
    float gentleMaxPct = 8.0f;
    float onduladoMaxPct = 20.0f; // New v3.4.1
    float steepMaxPct = 45.0f;
};

// Also common terrain model enum if needed?
enum class TerrainModel {
    FlatWithBumps = 0,
    GentleRolling = 1,
    Rolling = 2,
    Steep = 3
};

} // namespace graphics
