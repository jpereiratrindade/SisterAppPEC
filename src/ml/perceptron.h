#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>

namespace ml {

/**
 * @brief High-Performance Multilayer Perceptron for inference.
 * Uses Eigen for SIMD-accelerated matrix operations.
 */
class Perceptron {
public:
    // Constructor initializes weights randomly or to zero
    Perceptron(int inputSize);

    // Fast inference (no allocations)
    // Returns probability [0.0 - 1.0]
    float infer(const Eigen::VectorXf& input) const;

    // Load pre-trained weights from JSON
    bool load(const std::string& path);

    // Getters for inspection
    int getInputSize() const { return static_cast<int>(weights_.size()); }

    // On-device training (Backprop)
    void train(const Eigen::VectorXf& input, float target, float lr);

private:
    Eigen::VectorXf weights_;
    float bias_;

    // Activation function
    static float sigmoid(float z);
    static float sigmoidPrime(float z);
};

} // namespace ml
