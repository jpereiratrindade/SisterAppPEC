#pragma once

#include "terrain_map.h"
#include <cstdint>

namespace terrain {

class SoilPalette {
public:
    // Helper to get RGB components [0-255]
    static void getColor(SoilType type, uint8_t& r, uint8_t& g, uint8_t& b) {
        switch(type) {
            case SoilType::Raso:          r=178; g=178; b=51;  break; // Yellow-Green (0.7, 0.7, 0.2)
            case SoilType::BemDes:        r=128; g=38;  b=25;  break; // Reddish Brown (0.5, 0.15, 0.1)
            case SoilType::Hidromorfico:  r=0;   g=76;  b=76;  break; // Teal (0.0, 0.3, 0.3)
            case SoilType::Argila:        r=102; g=0;   b=127; break; // Purple (0.4, 0.0, 0.5)
            case SoilType::BTextural:     r=178; g=89;  b=13;  break; // Orange (0.7, 0.35, 0.05)
            case SoilType::Rocha:         r=51;  g=51;  b=51;  break; // Dark Gray (0.2, 0.2, 0.2)
            case SoilType::None:          r=255; g=0;   b=255; break; // Magenta (Debug)
            default:                      r=255; g=255; b=255; break; // White
        }
    }

    // Helper to get Packed Color for ImGui (0xAABBGGRR - Little Endian)
    static uint32_t getPackedColor(SoilType type, uint8_t alpha = 255) {
        uint8_t r, g, b;
        getColor(type, r, g, b);
        return (uint32_t(alpha) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
    }

    // Helper to get Normalized Float Color (0.0 - 1.0)
    static void getFloatColor(SoilType type, float* rgb) {
        uint8_t r, g, b;
        getColor(type, r, g, b);
        rgb[0] = r / 255.0f;
        rgb[1] = g / 255.0f;
        rgb[2] = b / 255.0f;
    }
};

} // namespace terrain
