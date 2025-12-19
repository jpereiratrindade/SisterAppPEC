#include "../src/ml/perceptron.h"
#include <iostream>
#include <Eigen/Dense>
#include <cassert>

int main() {
    std::cout << "[Test] ML Integration (Perceptron + Eigen)..." << std::endl;

    // 1. Initialize
    // Simple XOR-like logic (2 inputs)
    ml::Perceptron model(2);
    
    // 2. Mock Weights (Manual setting for test, usually loaded from JSON)
    // We cannot access private members directly, so we'll test the zero-init behavior
    // Default weights are zero. Sigmoid(0) = 0.5.
    
    Eigen::VectorXf input(2);
    input << 1.0f, 0.5f;

    float output = model.infer(input);
    
    std::cout << "Input: " << input.transpose() << " -> Output: " << output << std::endl;

    // Default bias=0, weights=0 => z=0 => sigmoid(0)=0.5
    assert(std::abs(output - 0.5f) < 0.0001f);
    
    std::cout << "[PASS] Inference ran successfully with Eigen backend." << std::endl;
    return 0;
}
