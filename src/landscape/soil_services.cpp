#include "soil_services.h"

#include <algorithm>
#include <cmath>

namespace landscape {

namespace {

ParentMaterial sanitize_parent_material(const ParentMaterial& parent_material) {
  ParentMaterial sanitized = parent_material;
  sanitized.weathering_rate = clamp01(sanitized.weathering_rate);
  sanitized.base_fertility = clamp01(sanitized.base_fertility);
  sanitized.sand_bias = clamp01(sanitized.sand_bias);
  sanitized.clay_bias = clamp01(sanitized.clay_bias);
  return sanitized;
}

Relief sanitize_relief(const Relief& relief) {
  Relief sanitized = relief;
  sanitized.slope = clamp_range(sanitized.slope, 0.0, 1.0);
  sanitized.curvature = clamp_range(sanitized.curvature, -1.0, 1.0);
  sanitized.slope_sensitivity = clamp01(sanitized.slope_sensitivity);
  sanitized.curvature_weight = clamp01(sanitized.curvature_weight);
  return sanitized;
}

Climate sanitize_climate(const Climate& climate) {
  Climate sanitized = climate;
  sanitized.rain_intensity = clamp01(sanitized.rain_intensity);
  sanitized.seasonality = clamp01(sanitized.seasonality);
  return sanitized;
}

OrganismPressure sanitize_organism_pressure(const OrganismPressure& pressure) {
  OrganismPressure sanitized = pressure;
  sanitized.max_cover = clamp01(sanitized.max_cover);
  sanitized.disturbance = clamp01(sanitized.disturbance);
  return sanitized;
}

}  // namespace

SoilState PedogenesisService::evolve(const SoilState& current,
                                    const ParentMaterial& parent_material,
                                    const Relief& relief,
                                    const Climate& climate,
                                    const OrganismPressure& organism_pressure,
                                    double dt) const {
  const ParentMaterial material = sanitize_parent_material(parent_material);
  const Relief topo = sanitize_relief(relief);
  const Climate climate_state = sanitize_climate(climate);
  const OrganismPressure pressure = sanitize_organism_pressure(organism_pressure);

  SoilState next = current;

  const double weathering = material.weathering_rate * (0.5 + climate_state.seasonality) * dt;
  const double curvature_factor = 0.5 + topo.curvature_weight * (0.5 - topo.curvature);
  const double erosion = topo.slope_sensitivity * topo.slope * climate_state.rain_intensity *
                         curvature_factor * dt;

  next.mineral.depth = std::max(0.0, current.mineral.depth + weathering - erosion);

  double target_sand = clamp01(material.sand_bias + 0.1 * (1.0 - topo.slope));
  double target_clay = clamp01(material.clay_bias + 0.1 * climate_state.seasonality +
                               0.05 * topo.curvature_weight);
  const double sum = target_sand + target_clay;
  if (sum > 1.0) {
    target_sand /= sum;
    target_clay /= sum;
  }

  const double blend = clamp01(dt) * 0.3;
  next.mineral.sand_fraction = lerp(current.mineral.sand_fraction, target_sand, blend);
  next.mineral.clay_fraction = lerp(current.mineral.clay_fraction, target_clay, blend);

  // v4.5.3: Reduced Litter Input to equate equilibrium at ~5% OM
  // Old: 0.2 base. New: 0.005 base.
  const double litter_input = pressure.max_cover * (0.005 + 0.01 * climate_state.seasonality) * dt;
  const double disturbance_loss = pressure.disturbance * dt;
  const double decomposition =
      (0.05 + 0.1 * (1.0 - climate_state.seasonality)) * current.organic.labile_carbon * dt;

  next.organic.labile_carbon = std::max(
      0.0, current.organic.labile_carbon + litter_input - decomposition - disturbance_loss);
  next.organic.recalcitrant_carbon = std::max(
      0.0, current.organic.recalcitrant_carbon +
                0.02 * current.organic.labile_carbon * dt -
                0.01 * current.organic.recalcitrant_carbon * dt);
  next.organic.dead_biomass = std::max(
      0.0, current.organic.dead_biomass + disturbance_loss -
                0.03 * current.organic.dead_biomass * dt);

  const double capacity = 0.1 + 0.4 * next.mineral.clay_fraction +
                          0.2 * next.organic.recalcitrant_carbon;
  next.hydric.field_capacity = std::max(0.05, capacity);
  next.hydric.conductivity = clamp_range(
      0.05 + 0.3 * next.mineral.sand_fraction - 0.2 * next.mineral.clay_fraction, 0.01, 1.0);

  const double infiltration = climate_state.rain_intensity *
                              (1.0 - topo.slope * topo.slope_sensitivity) * dt;
  const double evaporation = (0.02 + 0.05 * (1.0 - climate_state.seasonality)) * dt;
  next.hydric.water_content = clamp_range(
      current.hydric.water_content + infiltration - evaporation, 0.0, next.hydric.field_capacity);

  return next;
}

OrganismState EcologicalService::evolve(const OrganismState& current,
                                        const SoilState& soil_state,
                                        const Climate& climate,
                                        const OrganismPressure& organism_pressure,
                                        double dt) const {
  const Climate climate_state = sanitize_climate(climate);
  const OrganismPressure pressure = sanitize_organism_pressure(organism_pressure);

  OrganismState next = current;

  const double capacity = std::max(pressure.max_cover, kEpsilon);
  const double water_factor = clamp01(soil_state.hydric.water_content /
                                      std::max(soil_state.hydric.field_capacity, kEpsilon));
  const double fertility = clamp01(soil_state.organic.labile_carbon +
                                   soil_state.organic.recalcitrant_carbon);
  const double growth = water_factor * (0.5 + 0.5 * fertility) * (0.6 + 0.4 * climate_state.seasonality);

  const double grass_target = capacity * 0.6;
  const double shrub_target = capacity * 0.4;

  next.biomass_grass = clamp_range(
      current.biomass_grass + (grass_target - current.biomass_grass) * growth * dt -
          pressure.disturbance * dt,
      0.0,
      capacity);
  next.biomass_shrub = clamp_range(
      current.biomass_shrub + (shrub_target - current.biomass_shrub) * growth * dt -
          pressure.disturbance * 0.5 * dt,
      0.0,
      capacity);

  const double total_biomass = next.biomass_grass + next.biomass_shrub;
  next.roots_density = clamp01(total_biomass / capacity);

  return next;
}

ParentMaterial DataInjectionService::inject_parent_material(const ParentMaterial& current,
                                                            const ParentMaterial& incoming) const {
  (void)current;
  return sanitize_parent_material(incoming);
}

} // namespace landscape

// Implementation of New Services

namespace landscape {

void SoilPhysicsService::applyTopographyToTexture(SoilMineralState& mineral, const Relief& relief) const {
    // Logic: 
    // Steep slopes (High erosion) -> Lose fines (Clay/Silt) -> Relatively Sandier.
    // Concave/Flat (Deposition) -> Accumulate fines -> Relatively Clayier.

    double slopeMod = clamp_range(relief.slope * 0.3, 0.0, 0.3); // Max +30% Sand on very steep
    double curvatureMod = clamp_range(relief.curvature * -5.0, -0.2, 0.2); // Concave(+c) -> Deposition(-Sand), Convex(-c) -> Erosion(+Sand)

    // Note: Curvature definition varies. 
    // Here assuming: Positive Curvature = Concave (Valley) -> Accumulates Clay -> Reduce Sand.
    // Negative Curvature = Convex (Ridge) -> Erodes -> Increase Sand.
    
    // Apply changes primarily to Sand (Residue) vs Clay (Transportable)
    double targetSand = mineral.sand_fraction + slopeMod - curvatureMod;
    double targetClay = mineral.clay_fraction - slopeMod + curvatureMod;
    
    mineral.sand_fraction = clamp_range(targetSand, 0.05, 0.95);
    mineral.clay_fraction = clamp_range(targetClay, 0.05, 0.95);

    // Normalize
    double sum = mineral.sand_fraction + mineral.clay_fraction;
    if (sum > 0.95) {
        double scale = 0.95 / sum;
        mineral.sand_fraction *= scale;
        mineral.clay_fraction *= scale;
    }
}

SiBCSResult SiBCSClassifier::classify(const SoilState& state, const Relief& relief, SiBCSLevel targetLevel) const {
    SiBCSResult result;
    result.deepestLevel = SiBCSLevel::Order;

    // Level 1: Order
    result.order = determineOrder(state, relief);
    if (targetLevel == SiBCSLevel::Order) return result;

    // Level 2: Suborder
    result.suborder = determineSuborder(state, relief, result.order);
    result.deepestLevel = SiBCSLevel::Suborder;
    if (targetLevel == SiBCSLevel::Suborder) return result;

    // Level 3: Great Group
    result.greatGroup = determineGreatGroup(state, relief, result.order, result.suborder);
    result.deepestLevel = SiBCSLevel::GreatGroup;
    if (targetLevel == SiBCSLevel::GreatGroup) return result;

    // Level 4: Subgroup
    result.subGroup = determineSubGroup(state, relief, result.order, result.suborder, result.greatGroup);
    result.deepestLevel = SiBCSLevel::SubGroup;
    if (targetLevel == SiBCSLevel::SubGroup) return result;

    // Level 5: Family
    result.family = determineFamily(state);
    result.deepestLevel = SiBCSLevel::Family;
    if (targetLevel == SiBCSLevel::Family) return result;

    // Level 6: Series
    result.series = determineSeries(state);
    result.deepestLevel = SiBCSLevel::Series;
    
    return result;
}

SiBCSOrder SiBCSClassifier::determineOrder(const SoilState& state, const Relief& relief) const {
    const Relief topo = sanitize_relief(relief);

    // 1. Organossolo (High Carbon)
    double total_carbon = state.organic.labile_carbon + state.organic.recalcitrant_carbon;
    if (total_carbon > 0.08) return SiBCSOrder::kOrganossolo;

    // 2. Gleissolo (Hydromorphic)
    if (state.hydric.water_content >= state.hydric.field_capacity * 0.9 && 
        topo.slope < 0.03 && topo.curvature > 0.0) {
        return SiBCSOrder::kGleissolo;
    }

    // 3. Neossolo (Young/Shallow)
    if (state.mineral.depth < 0.5) return SiBCSOrder::kNeossoloLit;
    if (state.mineral.sand_fraction > 0.85 && state.mineral.depth >= 0.5) return SiBCSOrder::kNeossoloQuartz;

    // 4. Argissolo (Textural Gradient proxy)
    if (state.mineral.depth >= 0.8 && state.mineral.clay_fraction > 0.28) {
        return SiBCSOrder::kArgissolo;
    }

    // 5. Latossolo (Deep, Weathered)
    if (state.mineral.depth >= 1.5 &&
        state.mineral.clay_fraction >= 0.12 && state.mineral.clay_fraction <= 0.28 &&
        topo.slope <= 0.20) {
        return SiBCSOrder::kLatossolo;
    }

    return SiBCSOrder::kCambissolo;
}

SiBCSSubOrder SiBCSClassifier::determineSuborder(const SoilState& state, const Relief& /*relief*/, SiBCSOrder order) const {
    // Logic migrated from SoilSystem::initialize
    if (order == SiBCSOrder::kGleissolo) {
        double om = state.organic.labile_carbon + state.organic.recalcitrant_carbon;
        if (om > 0.03) return SiBCSSubOrder::kMelanico;
        return SiBCSSubOrder::kHaplic; // "Haplic" as Generic fallback for Gley
    }
    if (order == SiBCSOrder::kNeossoloLit) return SiBCSSubOrder::kLitolico;
    if (order == SiBCSOrder::kNeossoloQuartz) return SiBCSSubOrder::kQuartzarenico;
    
    if (order == SiBCSOrder::kArgissolo) {
        if (state.mineral.depth > 1.5 || state.mineral.clay_fraction > 0.6) return SiBCSSubOrder::kVermelho;
        return SiBCSSubOrder::kVermelhoAmarelo;
    }
    
    if (order == SiBCSOrder::kLatossolo) {
        if (state.mineral.sand_fraction > 0.4) return SiBCSSubOrder::kVermelhoAmarelo;
        if (state.mineral.sand_fraction < 0.2) return SiBCSSubOrder::kVermelho;
        return SiBCSSubOrder::kAmarelo;
    }

    if (order == SiBCSOrder::kOrganossolo) return SiBCSSubOrder::kMelanico;

    return SiBCSSubOrder::kHaplic;
}

SiBCSGreatGroup SiBCSClassifier::determineGreatGroup(const SoilState& state, const Relief& /*relief*/, SiBCSOrder order, SiBCSSubOrder /*suborder*/) const {
    // 1. Estimation of Base Saturation (V%) proxy derived from Clay + Organic Matter vs Leaching Risk
    // High Clay + High Organic usually retains more cations (Eutrophic)
    // Low Clay or High Leaching -> Dystrophic/Aluminic
    
    double fertilityIndex = (state.mineral.clay_fraction * 0.5) + (state.organic.labile_carbon * 20.0); 
    // Example: Clay 0.4 (0.2) + Carbon 0.02 (0.4) = 0.6 => Moderate
    
    // Eutrophic (High Fertility)
    if (fertilityIndex > 0.7) return SiBCSGreatGroup::kEutrofico;
    
    // Acric (Extremely Weathered, low activity clay) - Common in deep Latossols
    if (order == SiBCSOrder::kLatossolo && state.mineral.depth > 2.0 && state.mineral.clay_fraction > 0.6) {
        return SiBCSGreatGroup::kAcrico;
    }
    
    // Aluminic (High Al saturation, low pH)
    // Often associated with yellow/red argisols or cambisols in humid areas
    if (fertilityIndex < 0.3) {
        return SiBCSGreatGroup::kAluminico;
    }

    // Default Dystrophic for most weathered soils
    return SiBCSGreatGroup::kDistrofico;
}

SiBCSSubGroup SiBCSClassifier::determineSubGroup(const SoilState& state, const Relief& /*relief*/, SiBCSOrder order, SiBCSSubOrder /*suborder*/, SiBCSGreatGroup /*greatGroup*/) const {
    // Intergrades (Transition between orders)
    
    // Latossolico: Characteristics of Latossol (Deep, homogeneous) in other soils
    if (order != SiBCSOrder::kLatossolo && state.mineral.depth > 1.2 && state.mineral.clay_fraction > 0.3) {
        return SiBCSSubGroup::kLatossolico;
    }
    
    // Argissolico: Characteristics of Argissol (Texture gradient) in other soils
    if (order != SiBCSOrder::kArgissolo && state.mineral.clay_fraction > 0.35 && state.mineral.sand_fraction > 0.4) {
        // Crude proxy for textural gradient
        return SiBCSSubGroup::kArgissolico;
    }
    
    // Cambissolico: Young development in older soils? Or vice versa.
    if (state.mineral.depth < 0.8 && order != SiBCSOrder::kCambissolo && order != SiBCSOrder::kNeossoloLit) {
        return SiBCSSubGroup::kCambissolico;
    }
    
    return SiBCSSubGroup::kTipico;
}

SiBCSFamily SiBCSClassifier::determineFamily(const SoilState& state) const {
    if (state.mineral.clay_fraction > 0.60) return SiBCSFamily::kTexturaMuitoArgilosa;
    if (state.mineral.clay_fraction > 0.35) return SiBCSFamily::kTexturaArgilosa;
    if (state.mineral.sand_fraction > 0.70) return SiBCSFamily::kTexturaArenosa;
    return SiBCSFamily::kTexturaMedia;
}

SiBCSSeries SiBCSClassifier::determineSeries(const SoilState& state) const {
    // Synthetic Mapping for Visualization (Mocking local series)
    
    // 1. Hydromorphic Series
    if (state.hydric.water_content > state.hydric.field_capacity * 0.9) return SiBCSSeries::kVarzea;
    
    // 2. Lithic Series
    if (state.mineral.depth < 0.5) return SiBCSSeries::kSerra;
    
    // 3. Sandy Series
    if (state.mineral.sand_fraction > 0.8) return SiBCSSeries::kAreias;
    
    // 4. Clayey Series
    if (state.mineral.clay_fraction > 0.45) {
        // High fertility -> Ribeirão Preto (Purple Terra Roxa notion)
        double fertilityIndex = (state.mineral.clay_fraction * 0.5) + (state.organic.labile_carbon * 20.0);
        if (fertilityIndex > 0.7) return SiBCSSeries::kRibeiraoPreto;
        return SiBCSSeries::kCerradoNativo;
    }
    
    return SiBCSSeries::kGeneric;
}

} // namespace landscape
