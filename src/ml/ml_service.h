#pragma once
#include "perceptron.h"
#include <map>
#include <memory>
#include <string>
#include <Eigen/Dense>

namespace ml {

class MLService {
public:
    MLService();
    ~MLService();

    // Initialize the service (e.g., load config)
    void init();

    // Load a model from disk
    bool loadModel(const std::string& name, const std::string& path);

    // Specific PoC method: Predict Soil Color (R, G, B) based on 4 inputs (N, P, K, pH)
    // Returns Vector3f (R, G, B) in range [0, 1]
    Eigen::Vector3f predictSoilColor(float n, float p, float k, float ph) const;

private:
    std::map<std::string, std::unique_ptr<Perceptron>> models_;
};

} // namespace ml
