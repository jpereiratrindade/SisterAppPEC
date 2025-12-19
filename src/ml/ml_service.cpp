#include "ml_service.h"
#include <iostream>

namespace ml {

MLService::MLService() {}
MLService::~MLService() {}

void MLService::init() {
    std::cout << "[ML] Service Initialized." << std::endl;
}

bool MLService::loadModel(const std::string& name, const std::string& path) {
    // For PoC, assuming 4 inputs (N, P, K, pH)
    auto model = std::make_unique<Perceptron>(4); 
    if (model->load(path)) {
        models_[name] = std::move(model);
        std::cout << "[ML] Loaded model: " << name << std::endl;
        return true;
    }
    std::cerr << "[ML] Failed to load model: " << name << " from " << path << std::endl;
    return false;
}

Eigen::Vector3f MLService::predictSoilColor(float n, float p, float k, float ph) const {
    // 1. Prepare Input Vector
    Eigen::VectorXf input(4);
    input << n, p, k, ph;

    // 2. Get Model (if loaded)
    auto it = models_.find("soil_color");
    if (it != models_.end()) {
        float output = it->second->infer(input);
        
        // 3. Map single scalar output [0,1] to a Color Gradient (PoC)
        // output 0.0 -> Red (Poor/Low Score)
        // output 1.0 -> Green (Fertile/High Score)
        // We add a bit of Blue for style
        return Eigen::Vector3f(1.0f - output, output, 0.2f); 
    }

    // Fallback: If no model loaded, return Neutral Grey to distinguish from "Error Magenta"
    // Magenta is too aggressive for an uninitialized state in a tool like this.
    // Actually, let's use a debug color (Cyan) assuming "Technology" 
    return Eigen::Vector3f(0.0f, 1.0f, 1.0f);
}

} // namespace ml
