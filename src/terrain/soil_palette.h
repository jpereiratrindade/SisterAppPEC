#pragma once

#include "terrain_map.h"
#include <cstdint>

namespace terrain {

class SoilPalette {
public:
    // Helper to get RGB components [0-255]
    static void getColor(SoilType type, landscape::SoilSubOrder sub, uint8_t& r, uint8_t& g, uint8_t& b) {
        // 1. Suborder overrides (Level 2) are order-dependent.
        // Avoid global overrides that make "Argissolo Vermelho" look like "Latossolo Vermelho".
        if (sub != landscape::SoilSubOrder::None && sub != landscape::SoilSubOrder::Haplic) {
            switch (type) {
                case SoilType::Latossolo:
                    switch (sub) {
                        case landscape::SoilSubOrder::Vermelho:         r=166; g=38;  b=38;  return; // UI legend match
                        case landscape::SoilSubOrder::Amarelo:          r=217; g=191; b=64;  return;
                        case landscape::SoilSubOrder::Vermelho_Amarelo: r=191; g=115; b=38;  return;
                        default: break;
                    }
                    break;
                case SoilType::Argissolo:
                    // SiBCS legend uses a brownish tone for Argissolos regardless of "Vermelho/Amarelo" nuance here.
                    // Keep both Vermelho and Vermelho_Amarelo mapped to the same family to prevent confusion.
                    if (sub == landscape::SoilSubOrder::Vermelho || sub == landscape::SoilSubOrder::Vermelho_Amarelo || sub == landscape::SoilSubOrder::Amarelo) {
                        r=181; g=99; b=61; return; // UI legend match
                    }
                    break;
                default:
                    break;
            }

            // Cross-order suborders
            switch (sub) {
                case landscape::SoilSubOrder::Litolico:      r=120; g=115; b=110; return; // Grey-Brown (Rocky)
                case landscape::SoilSubOrder::Quartzarenico: r=230; g=224; b=209; return; // Pale Sand
                case landscape::SoilSubOrder::Melanico:      r=38;  g=38;  b=51;  return; // Dark (UI legend match)
                default: break;
            }
        }

        // 2. Fallback to Order Defaults (Level 1)
        switch(type) {
            case SoilType::Raso:          r=178; g=178; b=51;  break; // Yellow-Green
            case SoilType::BemDes:        r=128; g=38;  b=25;  break; // Reddish Brown
            case SoilType::Hidromorfico:  r=0;   g=76;  b=76;  break; // Teal
            case SoilType::Argila:        r=102; g=0;   b=127; break; // Purple
            case SoilType::BTextural:     r=178; g=89;  b=13;  break; // Orange
            case SoilType::Rocha:         r=51;  g=51;  b=51;  break; // Dark Gray
            
            // SiBCS Colors (Generic fallback)
            case SoilType::Latossolo:            r=170; g=80;  b=60;  break;
            case SoilType::Argissolo:            r=180; g=100; b=60;  break;
            case SoilType::Cambissolo:           r=140; g=110; b=70;  break;
            case SoilType::Neossolo_Litolico:    r=120; g=120; b=100; break;
            case SoilType::Neossolo_Quartzarenico: r=220; g=210; b=180; break;
            case SoilType::Gleissolo:            r=89;  g=115; b=140; break; // Grey-Blue (UI legend match)
            case SoilType::Organossolo:          r=40;  g=30;  b=30;  break;
            
            case SoilType::None:          r=255; g=0;   b=255; break;
            default:                      r=255; g=255; b=255; break;
        }
    }
    
    // Legacy overload for backward compatibility
    static void getColor(SoilType type, uint8_t& r, uint8_t& g, uint8_t& b) {
        getColor(type, landscape::SoilSubOrder::None, r, g, b);
    }

    // Helper to get Packed Color for ImGui (0xAABBGGRR - Little Endian)
    static uint32_t getPackedColor(SoilType type, landscape::SoilSubOrder sub = landscape::SoilSubOrder::None, uint8_t alpha = 255) {
        uint8_t r, g, b;
        getColor(type, sub, r, g, b);
        return (uint32_t(alpha) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
    }

    // Helper to get Normalized Float Color (0.0 - 1.0)
    static void getFloatColor(SoilType type, landscape::SoilSubOrder sub, float* rgb) {
        uint8_t r, g, b;
        getColor(type, sub, r, g, b);
        rgb[0] = r / 255.0f;
        rgb[1] = g / 255.0f;
        rgb[2] = b / 255.0f;
    }
    
    // Legacy overload for backward compatibility
    static void getFloatColor(SoilType type, float* rgb) {
        getFloatColor(type, landscape::SoilSubOrder::None, rgb);
    }
};

} // namespace terrain
