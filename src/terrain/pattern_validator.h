#pragma once

#include "terrain_map.h"
#include "landscape_metrics.h"
#include <map>
#include <string>

namespace terrain {

// DDD: Estados Válidos do Padrão Espacial
enum class ValidationState {
    Stable,         // Dentro dos envelopes
    UnderTension,   // Desvio leve (10-20%)
    InTransition,   // Mudança de regime ativa (não usado automaticamente ainda)
    Incompatible    // Ruptura estrutural (>20% de desvio)
};

// DDD: Assinatura Espacial Esperada
struct PatchPatternSignature {
    // Envelopes (Min-Max)
    float minLSI, maxLSI;
    float minCF, maxCF;
    float minRCC, maxRCC;
    
    // Configuração Opcional
    bool requiresConnectivity = true;
};

// DDD: Domain Service
class PatternIntegrityValidator {
public:
    // API Principal: Valida uma métrica observada contra a assinatura do tipo
    static ValidationState validate(SoilType type, const ClassMetrics& metrics);

    // Retorna a assinatura padrão para um tipo (Registry)
    static PatchPatternSignature getSignature(SoilType type);
    
    // v4.3.3: Allow Runtime Configuration
    static void setSignature(SoilType type, const PatchPatternSignature& sig);
    
    // Helper para converter estado em string/cor
    static std::string getStateName(ValidationState state);
    static void getStateColor(ValidationState state, float* r, float* g, float* b);
    
    // v4.3.1: Actionable Feedback
    static std::string getViolationReason(SoilType type, const ClassMetrics& metrics);

private:
    static bool isWithin(double val, double min, double max);
    
    // Internal Storage
    static std::map<SoilType, PatchPatternSignature> signatures_;
    static bool initialized_;
    static void ensureInitialized();
};

} // namespace terrain
