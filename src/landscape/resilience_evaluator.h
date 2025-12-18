#pragma once

#include <vector>
#include <deque>

namespace landscape {

    /**
     * @brief Evaluates the resilience of the ecosystem over time.
     * Resilience = Capacity to recover functionality after disturbance.
     * We measure this by tracking the recovery rate of Total Biomass after significant drops.
     */
    class ResilienceEvaluator {
    public:
        struct Metric {
            float biomass = 0.0f;
            float diversity = 0.0f; // Functional diversity (EI vs ES balance)
            float soil_integrity = 0.0f; // Inverse of erosion
        };

        void update(float dt, float currentBiomass, float currentDiversity, float currentSoilIntegrity);
        void reset();

        // Getters for UI/Analysis
        float getCurrentResilienceScore() const { return lastResilienceScore_; }
        const std::deque<float>& getBiomassHistory() const { return biomassHistory_; }
        bool isRecovering() const { return isRecovering_; }

    private:
        std::deque<float> biomassHistory_; // Rolling window (e.g., last 60 seconds)
        float lastResilienceScore_ = 1.0f; // [0.0 - 1.0], 1.0 = Instant recovery
        
        // Resilience State Machine
        bool isRecovering_ = false;
        float preDisturbanceBiomass_ = 0.0f;
        float timeSinceDisturbance_ = 0.0f;
        
        static constexpr size_t MAX_HISTORY = 600; // 10s at 60fps, or 60s at 10Hz
    };

} // namespace landscape
