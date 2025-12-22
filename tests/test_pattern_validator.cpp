#include "../src/terrain/pattern_validator.h"
#include "../src/terrain/landscape_metrics.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace terrain;

int main() {
    std::cout << "[Test] PatternIntegrityValidator Logic..." << std::endl;

    // Use Raso as test subject
    // Signatures: LSI[1.0, 50.0], CF[0.0, 5.0], RCC[0.0, 1.0] needsConnectivity=true
    SoilType type = SoilType::Raso;
    
    // 1. STABLE Case
    {
        ClassMetrics m;
        m.pixelCount = 100;
        m.LSI = 25.0; // Perfect middle
        m.CF = 2.5;   // Perfect middle
        m.RCC = 0.5;  // Perfect middle
        
        ValidationState s = PatternIntegrityValidator::validate(type, m);
        assert(s == ValidationState::Stable);
        std::cout << "[PASS] Stable case verified." << std::endl;
    }

    // 2. UNDER TENSION (LSI)
    // Range LSI = 49.0. 10% dev = 4.9. 30% dev = 14.7.
    // Let's try LSI = 60.0. Dev = (60-50)/49 = 10/49 = ~0.20
    // Should be UnderTension (<0.3)
    {
        ClassMetrics m;
        m.pixelCount = 100;
        m.LSI = 60.0; 
        m.CF = 2.5;
        m.RCC = 0.5;
        
        ValidationState s = PatternIntegrityValidator::validate(type, m);
        if (s != ValidationState::UnderTension) {
             std::cout << "[FAIL] UnderTension (LSI) - Expected UnderTension, got: " << PatternIntegrityValidator::getStateName(s) << std::endl;
        }
        assert(s == ValidationState::UnderTension);
        std::cout << "[PASS] UnderTension (LSI) verified." << std::endl;
    }

    // 3. IN TRANSITION (Mixed Signals)
    // LSI = 60.0 (Dev ~0.2) -> UT
    // CF = 6.0 (Range 5.0. Dist 1.0. 1/5 = 0.2) -> UT
    // 2 metrics under tension -> InTransition
    {
        ClassMetrics m;
        m.pixelCount = 100;
        m.LSI = 60.0;
        m.CF = 6.0;
        m.RCC = 0.5;
        
        ValidationState s = PatternIntegrityValidator::validate(type, m);
        if (s != ValidationState::InTransition) {
             std::cout << "[FAIL] Mixed Signals - Expected InTransition, got: " << PatternIntegrityValidator::getStateName(s) << std::endl;
        }
        assert(s == ValidationState::InTransition);
        std::cout << "[PASS] InTransition (Mixed) verified." << std::endl;
    }

    // 4. IN TRANSITION (Asymmetric / High but not broken)
    // LSI = 70.0. Dist 20. 20/49 = ~0.40. (>0.3 but <0.5)
    // CF = 2.5 (Stable)
    // RCC = 0.5 (Stable)
    // MetricsOff = 1.
    // Logic: if (metricsOff < 3 && (lsiDev < 0.5 ...)) -> InTransition
    {
        ClassMetrics m;
        m.pixelCount = 100;
        m.LSI = 70.0; 
        m.CF = 2.5;
        m.RCC = 0.5;

        ValidationState s = PatternIntegrityValidator::validate(type, m);
        if (s != ValidationState::InTransition) {
             std::cout << "[FAIL] Asymmetric - Expected InTransition, got: " << PatternIntegrityValidator::getStateName(s) << std::endl;
        }
        assert(s == ValidationState::InTransition);
        std::cout << "[PASS] InTransition (High Deviation) verified." << std::endl;
    }

    // 5. INCOMPATIBLE
    // LSI = 100.0. Dist 50. 50/49 = > 1.0.
    {
        ClassMetrics m;
        m.pixelCount = 100;
        m.LSI = 100.0;
        m.CF = 2.5;
        m.RCC = 0.5;

        ValidationState s = PatternIntegrityValidator::validate(type, m);
        assert(s == ValidationState::Incompatible);
        std::cout << "[PASS] Incompatible verified." << std::endl;
    }

    // 6. Semantic Check (Too Small)
    {
        ClassMetrics m;
        m.pixelCount = 5;
        std::string reason = PatternIntegrityValidator::getViolationReason(type, m);
        if (reason != "Below Ecological Resolution") {
             std::cout << "[FAIL] String - Expected 'Below Ecological Resolution', got: " << reason << std::endl;
        }
        assert(reason == "Below Ecological Resolution");
        std::cout << "[PASS] Semantic string verified." << std::endl;
    }

    return 0;
}
