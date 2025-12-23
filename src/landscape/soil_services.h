#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace landscape {

// Math Utilities
constexpr double kEpsilon = 1e-6;

inline double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

inline double clamp_range(double value, double min_value, double max_value) {
  return std::clamp(value, min_value, max_value);
}

inline double lerp(double a, double b, double t) {
  return a + (b - a) * t;
}

// Data Structures (from vectors.h)

struct ParentMaterial {
  double weathering_rate = 0.3;
  double base_fertility = 0.5;
  double sand_bias = 0.4;
  double clay_bias = 0.2;
};

struct Relief {
  double elevation = 0.0;
  double slope = 0.2;
  double aspect = 0.0;
  double curvature = 0.0;
  double slope_sensitivity = 0.7;
  double curvature_weight = 0.5;
};

struct Climate {
  double rain_intensity = 0.6;
  double seasonality = 0.5;
};

struct OrganismPressure {
  double max_cover = 0.6;
  double disturbance = 0.1;
};

struct SoilMineralState {
  double depth = 0.2;
  double sand_fraction = 0.4;
  double clay_fraction = 0.2;

  double silt_fraction() const {
    return clamp_range(1.0 - sand_fraction - clay_fraction, 0.0, 1.0);
  }
};

struct SoilOrganicState {
  double labile_carbon = 0.1;
  double recalcitrant_carbon = 0.05;
  double dead_biomass = 0.02;
};

struct SoilHydricState {
  double water_content = 0.1;
  double field_capacity = 0.2;
  double conductivity = 0.05;
};

struct SoilState {
  SoilMineralState mineral;
  SoilOrganicState organic;
  SoilHydricState hydric;
};

struct OrganismState {
  double biomass_grass = 0.1;
  double biomass_shrub = 0.05;
  double roots_density = 0.2;
};

enum class TextureClass {
  kSand,
  kLoam,
  kSilt,
  kClay,
  kSandyLoam, // v4.5.6
  kClayLoam,  // v4.5.6
  kSandyClayLoam, // v4.5.7
  kSiltyClayLoam,
  kSandyClay,
  kSiltyClay,
  kLoamySand,
  kSiltLoam,
  kUnknown
};

// SiBCS Types (Sistema Brasileiro de Classificação de Solos - 5ª Edição, 2018)
enum class SiBCSOrder {
    kLatossolo = 0,
    kArgissolo = 1,
    kCambissolo = 2,
    kNeossoloLit = 3,  // Litolico
    kNeossoloQuartz = 4, // Quartzarenico
    kGleissolo = 5,
    kOrganossolo = 6,
    kPlintossolo = 7,  // Optional
    kEspodossolo = 8,  // Optional
    kVertissolo = 9,   // Optional
    kPlanossolo = 10,  // Optional
    kChernossolo = 11, // Optional
    kNitossolo = 12,    // Optional
    kLuvissolo = 13,   // Optional
    kNone = 255
};

inline TextureClass classify_texture(const SoilMineralState& mineral) {
  // USDA Approximation (Gapless Decision Tree)
  // 1. Extreme Textures
  if (mineral.sand_fraction >= 0.85) return TextureClass::kSand;
  if (mineral.clay_fraction >= 0.40) {
      if (mineral.sand_fraction >= 0.45) return TextureClass::kSandyClay;
      if (mineral.silt_fraction() >= 0.40) return TextureClass::kSiltyClay;
      return TextureClass::kClay;
  }
  
  // 2. High Clay (27% - 40%) - The "Clay Loam" Band
  if (mineral.clay_fraction >= 0.27) {
      if (mineral.sand_fraction >= 0.45) return TextureClass::kSandyClayLoam; // User's Case (Sand~48, Clay~30)
      if (mineral.silt_fraction() >= 0.60) return TextureClass::kSiltyClayLoam;
      return TextureClass::kClayLoam;
  }
  
  // 3. Low Clay (< 27%)
  if (mineral.silt_fraction() >= 0.50) {
      if (mineral.clay_fraction >= 0.12) return TextureClass::kSiltLoam; // Common
      return TextureClass::kSilt; // Very Silty
  }
  
  if (mineral.sand_fraction >= 0.50) { // Was 0.45, typically Sandy Loam starts >50% Sand if Clay is low
      if (mineral.sand_fraction >= 0.70) return TextureClass::kLoamySand;
      return TextureClass::kSandyLoam;
  }

  // 4. Center
  return TextureClass::kLoam;
}

// Services (from services.h)

class PedogenesisService {
 public:
  SoilState evolve(const SoilState& current,
                   const ParentMaterial& parent_material,
                   const Relief& relief,
                   const Climate& climate,
                   const OrganismPressure& organism_pressure,
                   double dt) const;
};

class EcologicalService {
 public:
  OrganismState evolve(const OrganismState& current,
                       const SoilState& soil_state,
                       const Climate& climate,
                       const OrganismPressure& organism_pressure,
                       double dt) const;
};

class DataInjectionService {
 public:
  ParentMaterial inject_parent_material(const ParentMaterial& current,
                                        const ParentMaterial& incoming) const;
};

// Domain Services (DDD)

/**
 * @brief Handles pure physical/chemical derivations based on environment.
 * Responsible for applying topography effects to texture (Mechanistic).
 */
class SoilPhysicsService {
public:
    // Mechanistic: Modifies sand/clay fractions based on relief (Erosion/Deposition)
    void applyTopographyToTexture(SoilMineralState& mineral, const Relief& relief) const;
};

/**
 * @brief Handles taxonomic classification (Diagnostic).
 * Pure function: State -> Class Name.
 */
class SiBCSClassifier {
public:
    SiBCSOrder classify(const SoilState& state, const Relief& relief) const;
};

} // namespace landscape
