#include "../src/ml/ml_service.h"
#include <iostream>
#include <cassert>
#include <Eigen/Dense>

int main() {
    std::cout << "[Test] MLService Integration..." << std::endl;

    // 1. Init
    ml::MLService service;
    service.init();

    // 2. Load Dummy Model
    // Valid path check (test runs from root usually)
    bool loaded = service.loadModel("soil_color", "assets/models/soil_color.json");
    if (!loaded) {
        std::cerr << "[FAIL] Could not load assets/models/soil_color.json" << std::endl;
        return 1;
    }

    // 3. Predict
    // Inputs: Depth=1.0, OM=0.5, Inf=0.5, Comp=0.0
    // Model: weights=[0.5, 0.5, 0.5, 0.5], bias=-1.0
    // Sum = 0.5 + 0.25 + 0.25 + 0.0 = 1.0
    // z = 1.0 - 1.0 = 0.0
    // sigmoid(0) = 0.5
    
    // Pred Color: (1-0.5, 0.5, 0.2) = (0.5, 0.5, 0.2)
    
    Eigen::Vector3f color = service.predictSoilColor(1.0f, 0.5f, 50.0f / 100.0f, 0.0f);
    
    std::cout << "Predicted Color: " << color.transpose() << std::endl;

    assert(std::abs(color.x() - 0.5f) < 0.001f);
    assert(std::abs(color.y() - 0.5f) < 0.001f);
    assert(std::abs(color.z() - 0.2f) < 0.001f);

    std::cout << "[PASS] MLService logic verified." << std::endl;
    return 0;
}
