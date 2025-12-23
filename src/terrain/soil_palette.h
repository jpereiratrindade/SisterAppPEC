#pragma once

#include "terrain_map.h"
#include <cstdint>

namespace terrain {

class SoilPalette {
public:
    // Helper to get RGB components [0-255]
    static void getColor(SoilType type, uint8_t& r, uint8_t& g, uint8_t& b) {
        switch(type) {
            case SoilType::Raso:          r=178; g=178; b=51;  break; // Yellow-Green
            case SoilType::BemDes:        r=128; g=38;  b=25;  break; // Reddish Brown
            case SoilType::Hidromorfico:  r=0;   g=76;  b=76;  break; // Teal
            case SoilType::Argila:        r=102; g=0;   b=127; break; // Purple
            case SoilType::BTextural:     r=178; g=89;  b=13;  break; // Orange
            case SoilType::Rocha:         r=51;  g=51;  b=51;  break; // Dark Gray
            
            // SiBCS Colors
            case SoilType::Latossolo:            r=160; g=60;  b=40;  break; // Deep Red (Oxidized iron)
            case SoilType::Argissolo:            r=180; g=100; b=60;  break; // Red-Yellow (Clay accumulation)
            case SoilType::Cambissolo:           r=140; g=110; b=70;  break; // Brown (Incipient)
            case SoilType::Neossolo_Litolico:    r=120; g=120; b=100; break; // Greyish-Brown (Shallow/Rocky)
            case SoilType::Neossolo_Quartzarenico: r=220; g=210; b=180; break; // Pale Yellow/White (Sand)
            case SoilType::Gleissolo:            r=80;  g=100; b=120; break; // Blue-Grey (Reduced iron)
            case SoilType::Organossolo:          r=40;  g=30;  b=30;  break; // Very Dark Brown/Black
            
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
