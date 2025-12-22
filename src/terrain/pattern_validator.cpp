#include "pattern_validator.h"
#include <cmath>

namespace terrain {

// Static Init
std::map<SoilType, PatchPatternSignature> PatternIntegrityValidator::signatures_;
bool PatternIntegrityValidator::initialized_ = false;

void PatternIntegrityValidator::ensureInitialized() {
    if (initialized_) return;
    
    // Initialize Defaults
    // Raso
    signatures_[SoilType::Raso] = {1.0, 50.0, 0.0, 5.0, 0.0, 1.0, true};
    // BemDes
    signatures_[SoilType::BemDes] = {1.0, 20.0, 0.0, 1.5, 0.0, 1.0, true};
    // Hidromorfico
    signatures_[SoilType::Hidromorfico] = {1.0, 100.0, 0.0, 10.0, 0.0, 1.0, true};
    // BTextural
    signatures_[SoilType::BTextural] = {1.0, 40.0, 0.0, 3.0, 0.0, 1.0, true};
    // Argila/Rocha
    signatures_[SoilType::Argila] = {1.0, 40.0, 0.0, 5.0, 0.0, 1.0, true};
    signatures_[SoilType::Rocha] = {1.0, 40.0, 0.0, 5.0, 0.0, 1.0, true};
    
    initialized_ = true;
}

PatchPatternSignature PatternIntegrityValidator::getSignature(SoilType type) {
    ensureInitialized();
    if (signatures_.count(type)) return signatures_[type];
    return {0.0, 999.0, 0.0, 999.0, 0.0, 1.0, false};
}

void PatternIntegrityValidator::setSignature(SoilType type, const PatchPatternSignature& sig) {
    ensureInitialized();
    signatures_[type] = sig;
}

bool PatternIntegrityValidator::isWithin(double val, double min, double max) {
    return val >= min && val <= max;
}

ValidationState PatternIntegrityValidator::validate(SoilType type, const ClassMetrics& metrics) {
    // v4.3.5: Refined logic - semantic check
    if (metrics.pixelCount < 50) return ValidationState::Stable;

    auto sig = getSignature(type);
    
    // Helper to calculate deviation relative to range (max - min)
    // Returns 0.0 if within range, > 0.0 if outside
    auto calcDev = [](double val, double min, double max) -> double {
        if (val >= min && val <= max) return 0.0;
        double range = max - min;
        if (range < 0.0001) range = 1.0; // Avoid div by zero
        double dist = std::min(std::abs(val - min), std::abs(val - max));
        return dist / range;
    };

    // 1. Calculate Deviations
    double lsiDev = calcDev(metrics.LSI, sig.minLSI, sig.maxLSI);
    double cfDev = calcDev(metrics.CF, sig.minCF, sig.maxCF);
    double rccDev = calcDev(metrics.RCC, sig.minRCC, sig.maxRCC);

    // 2. Decision Logic
    // All clean
    if (lsiDev == 0.0 && cfDev == 0.0 && rccDev == 0.0) return ValidationState::Stable;

    // Check for InTransition conditions (Conflicting signals)
    // E.g., LSI is perfect but CF is slightly off, or vice versa
    // Or RCC is starting to drift while others are fine.
    int metricsOff = (lsiDev > 0) + (cfDev > 0) + (rccDev > 0);
    
    // Moderate Tension (UnderTension)
    // Allow slightly higher tolerance if it's just one metric or low aggregate deviation
    // Thresholds: 0.3 (30% of range) is a reasonable "tension" zone
    // (Variables removed to suppress warnings)

    // If strictly UnderTension (small deviations)
    if (lsiDev < 0.3 && cfDev < 0.3 && rccDev < 0.3) {
        // If 2 or more metrics are under tension, or one is stable and others tension
        // Use InTransition to indicate active reshaping
        if (metricsOff >= 2) return ValidationState::InTransition;
        return ValidationState::UnderTension;
    }

    // High Deviation but asymmetric (one metric very bad, others good) -> Transitioning/Breaking
    if (metricsOff < 3 && (lsiDev < 0.5 && cfDev < 0.5 && rccDev < 0.5)) {
         return ValidationState::InTransition;
    }
    
    // Otherwise, structural collapse
    return ValidationState::Incompatible;
}

std::string PatternIntegrityValidator::getStateName(ValidationState state) {
    switch(state) {
        case ValidationState::Stable: return "Stable";
        case ValidationState::UnderTension: return "Under Tension";
        case ValidationState::InTransition: return "In Transition";
        case ValidationState::Incompatible: return "Incompatible";
    }
    return "Unknown";
}

void PatternIntegrityValidator::getStateColor(ValidationState state, float* r, float* g, float* b) {
    switch(state) {
        case ValidationState::Stable: 
            *r=0.0f; *g=1.0f; *b=0.0f; break; // Green
        case ValidationState::UnderTension:
            *r=1.0f; *g=1.0f; *b=0.0f; break; // Yellow
        case ValidationState::InTransition:
            *r=1.0f; *g=0.5f; *b=0.0f; break; // Orange
        case ValidationState::Incompatible:
            *r=1.0f; *g=0.0f; *b=0.0f; break; // Red
    }
}

std::string PatternIntegrityValidator::getViolationReason(SoilType type, const ClassMetrics& metrics) {
    if (metrics.pixelCount < 50) return "Below Ecological Resolution";
    
    auto sig = getSignature(type);
    std::string reason = "";
    
    if (!isWithin(metrics.LSI, sig.minLSI, sig.maxLSI)) {
        if (metrics.LSI < sig.minLSI) reason += "LSI Low (inc. Roughness); ";
        else reason += "LSI High (red. Roughness); ";
    }
    
    if (!isWithin(metrics.CF, sig.minCF, sig.maxCF)) {
        if (metrics.CF < sig.minCF) reason += "CF Low (make irregular); ";
        else reason += "CF High (simplify); ";
    }

    if (!isWithin(metrics.RCC, sig.minRCC, sig.maxRCC)) {
        reason += "Bad Shape (RCC); ";
    }
    
    if (reason.empty()) return "Stable";
    return reason;
}

} // namespace terrain
