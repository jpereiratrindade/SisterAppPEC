#pragma once

#include "terrain_map.h"
#include <cstdint>

namespace terrain {

class SoilPalette {
public:
    // Helper to get RGB components [0-255]
    static void getColor(SoilType type, landscape::SiBCSSubOrder sub, uint8_t& r, uint8_t& g, uint8_t& b) {
        // 1. Suborder overrides (Level 2) are order-dependent.
        // Avoid global overrides that make "Argissolo Vermelho" look like "Latossolo Vermelho".
        if (sub != landscape::SiBCSSubOrder::kNone && sub != landscape::SiBCSSubOrder::kHaplic) {
            switch (type) {
                case SoilType::Latossolo:
                    switch (sub) {
                        case landscape::SiBCSSubOrder::kVermelho:         r=166; g=38;  b=38;  return; // UI legend match
                        case landscape::SiBCSSubOrder::kAmarelo:          r=217; g=191; b=64;  return;
                        case landscape::SiBCSSubOrder::kVermelhoAmarelo: r=191; g=115; b=38;  return;
                        default: break;
                    }
                    break;
                case SoilType::Argissolo:
                    // SiBCS legend uses a brownish tone for Argissolos regardless of "Vermelho/Amarelo" nuance here.
                    // Keep both Vermelho and Vermelho_Amarelo mapped to the same family to prevent confusion.
                    if (sub == landscape::SiBCSSubOrder::kVermelho || sub == landscape::SiBCSSubOrder::kVermelhoAmarelo || sub == landscape::SiBCSSubOrder::kAmarelo) {
                        r=181; g=99; b=61; return; // UI legend match
                    }
                    break;
                default:
                    break;
            }

            // Cross-order suborders
            switch (sub) {
                case landscape::SiBCSSubOrder::kLitolico:      r=120; g=115; b=110; return; // Grey-Brown (Rocky)
                case landscape::SiBCSSubOrder::kQuartzarenico: r=230; g=224; b=209; return; // Pale Sand
                case landscape::SiBCSSubOrder::kMelanico:      r=38;  g=38;  b=51;  return; // Dark (UI legend match)
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
        getColor(type, landscape::SiBCSSubOrder::kNone, r, g, b);
    }

    // Helper to get Packed Color for ImGui (0xAABBGGRR - Little Endian)
    static uint32_t getPackedColor(SoilType type, landscape::SiBCSSubOrder sub = landscape::SiBCSSubOrder::kNone, uint8_t alpha = 255) {
        uint8_t r, g, b;
        getColor(type, sub, r, g, b);
        return (uint32_t(alpha) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
    }

    // Helper to get Normalized Float Color (0.0 - 1.0)
    static void getFloatColor(SoilType type, landscape::SiBCSSubOrder sub, float* rgb) {
        uint8_t r, g, b;
        getColor(type, sub, r, g, b);
        rgb[0] = r / 255.0f;
        rgb[1] = g / 255.0f;
        rgb[2] = b / 255.0f;
    }
    
    // Legacy overload for backward compatibility
    static void getFloatColor(SoilType type, float* rgb) {
        getFloatColor(type, landscape::SiBCSSubOrder::kNone, rgb);
    }

    // Level 3: Great Group
    static void getFloatColor(landscape::SiBCSGreatGroup group, float* rgb) {
        switch(group) {
            case landscape::SiBCSGreatGroup::kEutrofico:  rgb[0]=0.5f; rgb[1]=0.2f; rgb[2]=0.2f; break; // Dark Red
            case landscape::SiBCSGreatGroup::kDistrofico: rgb[0]=0.8f; rgb[1]=0.7f; rgb[2]=0.4f; break; // Yellowish
            case landscape::SiBCSGreatGroup::kAluminico:  rgb[0]=0.7f; rgb[1]=0.7f; rgb[2]=0.8f; break; // Grey-Blue hint
            case landscape::SiBCSGreatGroup::kAcrico:     rgb[0]=0.9f; rgb[1]=0.4f; rgb[2]=0.3f; break; // Pale Red
            case landscape::SiBCSGreatGroup::kTipico:     rgb[0]=0.6f; rgb[1]=0.6f; rgb[2]=0.6f; break; // Grey
            default:                                      rgb[0]=0.5f; rgb[1]=0.5f; rgb[2]=0.5f; break;
        }
    }

    // Level 4: SubGroup
    static void getFloatColor(landscape::SiBCSSubGroup sub, float* rgb) {
        switch(sub) {
            case landscape::SiBCSSubGroup::kTipico:       rgb[0]=0.6f; rgb[1]=0.6f; rgb[2]=0.6f; break; 
            case landscape::SiBCSSubGroup::kLatossolico:  rgb[0]=0.7f; rgb[1]=0.3f; rgb[2]=0.2f; break;
            case landscape::SiBCSSubGroup::kArgissolico:  rgb[0]=0.7f; rgb[1]=0.5f; rgb[2]=0.3f; break;
            case landscape::SiBCSSubGroup::kCambissolico: rgb[0]=0.6f; rgb[1]=0.5f; rgb[2]=0.4f; break;
            default:                                      rgb[0]=0.8f; rgb[1]=0.8f; rgb[2]=0.8f; break;
        }
    }

    // Level 5: Family (Texture)
    static void getFloatColor(landscape::SiBCSFamily family, float* rgb) {
        switch(family) {
            case landscape::SiBCSFamily::kTexturaMuitoArgilosa: rgb[0]=0.4f; rgb[1]=0.0f; rgb[2]=0.5f; break; // Deep Purple
            case landscape::SiBCSFamily::kTexturaArgilosa:      rgb[0]=0.6f; rgb[1]=0.2f; rgb[2]=0.6f; break; // Purple
            case landscape::SiBCSFamily::kTexturaMedia:         rgb[0]=0.8f; rgb[1]=0.6f; rgb[2]=0.2f; break; // Loam Color
            case landscape::SiBCSFamily::kTexturaArenosa:       rgb[0]=0.9f; rgb[1]=0.9f; rgb[2]=0.6f; break; // Sand Color
            default:                                            rgb[0]=0.5f; rgb[1]=0.5f; rgb[2]=0.5f; break;
        }
    }

    // Level 6: Series
    static void getFloatColor(landscape::SiBCSSeries series, float* rgb) {
        if (series == landscape::SiBCSSeries::kGeneric) {
            rgb[0]=0.4f; rgb[1]=0.7f; rgb[2]=0.4f; // Generic Greenish
        } else {
            rgb[0]=0.5f; rgb[1]=0.5f; rgb[2]=0.5f;
        }
    }

    /**
     * @brief Cumulative Visualization (Hierarchical)
     * Applies modifiers from deeper taxonomic levels to the base Order/Suborder color.
     */
    static void getCumulativeColor(
        landscape::SiBCSLevel viewLevel,
        terrain::SoilType type,
        landscape::SiBCSSubOrder sub,
        landscape::SiBCSGreatGroup group,
        landscape::SiBCSSubGroup subGroup,
        landscape::SiBCSFamily family,
        landscape::SiBCSSeries series,
        float* rgb) 
    {
        // 1. Base Color (Levels 1 & 2)
        getFloatColor(type, sub, rgb);
        
        // Handle "Bruno" Suborder override
        if (sub == landscape::SiBCSSubOrder::kBruno) {
             rgb[0] = 0.55f; rgb[1] = 0.45f; rgb[2] = 0.35f; // Brownish
        }

        if (viewLevel <= landscape::SiBCSLevel::Suborder) return;

        // Convert to HSV for modifiers
        float h, s, v;
        RGBtoHSV(rgb[0], rgb[1], rgb[2], h, s, v);

        // 2. Great Group Modifiers (Level 3)
        if (viewLevel >= landscape::SiBCSLevel::GreatGroup) {
            switch (group) {
                case landscape::SiBCSGreatGroup::kEutrofico:  s *= 1.2f; v *= 0.9f; break; // Richer
                case landscape::SiBCSGreatGroup::kDistrofico: s *= 0.8f; v *= 1.1f; break; // Paler
                case landscape::SiBCSGreatGroup::kAluminico:  h += 20.0f; s *= 0.7f; break; // Blue/Grey shift
                case landscape::SiBCSGreatGroup::kAcrico:     s *= 0.5f; v *= 1.2f; break; // Very pale (weathered)
                case landscape::SiBCSGreatGroup::kFerrico:    h = 0.0f; s = 0.9f; v *= 0.8f; break; // Deep Red Saturation
                case landscape::SiBCSGreatGroup::kOrtico:     /* Standard, no change */ break;
                default: break;
            }
        }

        // 3. SubGroup Modifiers (Level 4)
        if (viewLevel >= landscape::SiBCSLevel::SubGroup) {
             switch (subGroup) {
                case landscape::SiBCSSubGroup::kLatossolico: s *= 1.1f; break; 
                case landscape::SiBCSSubGroup::kArgissolico: v *= 0.95f; break;
                case landscape::SiBCSSubGroup::kCambissolico: s *= 0.9f; break;
                case landscape::SiBCSSubGroup::kPsamitico:    s *= 0.6f; v += 0.1f; break; // Sandy appearance (Pale/Desat)
                case landscape::SiBCSSubGroup::kHumico:       v *= 0.7f; break; // Darker (Carbon)
                default: break;
            }
        }

        // 4. Family Modifiers (Level 5) - Texture hints
        // Clayey -> Warm shift, Sandy -> Yellow shift
        if (viewLevel >= landscape::SiBCSLevel::Family) {
             switch (family) {
                case landscape::SiBCSFamily::kTexturaMuitoArgilosa: h -= 5.0f; s *= 1.1f; break; // Redder, more sat
                case landscape::SiBCSFamily::kTexturaArgilosa:      h -= 2.0f; break;
                case landscape::SiBCSFamily::kTexturaArenosa:       h += 10.0f; s *= 0.8f; break; // Yellower, less sat
                default: break;
            }
        }

        // 5. Series Modifiers (Level 6) - Specific local tinting/branding
        if (viewLevel >= landscape::SiBCSLevel::Series) {
             // To ensure Series are visually distinct from Level 5, we apply contrast/hue shifts
             switch (series) {
                case landscape::SiBCSSeries::kRibeiraoPreto: 
                    // Tonal shift to "Roxo" (Purple/Dark Red)
                    h = 340.0f; s = 0.8f; v *= 0.8f; // Deep Purple-Red
                    break;
                case landscape::SiBCSSeries::kCerradoNativo:
                    // Ochre / Rusty
                    h = 25.0f; s = 0.6f; v = 0.7f;
                    break;
                case landscape::SiBCSSeries::kAreias:
                    // Bright White/Yellow
                    h = 50.0f; s = 0.2f; v = 0.95f;
                    break;
                case landscape::SiBCSSeries::kVarzea:
                    // Blueish Grey
                    h = 200.0f; s = 0.3f; v = 0.6f;
                    break;
                case landscape::SiBCSSeries::kSerra:
                    // Dark Grey Rock
                    s = 0.1f; v = 0.4f;
                    break;
                default: break;
            }
        }

        // Re-clamp HSV
        if (h < 0.0f) h += 360.0f;
        if (h > 360.0f) h -= 360.0f;
        s = (s > 1.0f) ? 1.0f : ((s < 0.0f) ? 0.0f : s);
        v = (v > 1.0f) ? 1.0f : ((v < 0.0f) ? 0.0f : v);

        HSVtoRGB(h, s, v, rgb[0], rgb[1], rgb[2]);
    }

private:
   static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
        float min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
        float max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
        float delta = max - min;

        v = max;
        if (delta < 0.00001f) {
            s = 0; h = 0; 
            return;
        }
        s = delta / max;

        if (r >= max) h = (g - b) / delta;
        else if (g >= max) h = 2.0f + (b - r) / delta;
        else h = 4.0f + (r - g) / delta;

        h *= 60.0f;
        if (h < 0.0f) h += 360.0f;
    }

    static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
        if (s <= 0.0f) { r = v; g = v; b = v; return; }
        
        h /= 60.0f;
        int i = (int)h;
        float f = h - static_cast<float>(i);
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        switch(i) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }
};

} // namespace terrain
