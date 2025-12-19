#include "perceptron.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ml {

Perceptron::Perceptron(int inputSize) 
    : weights_(Eigen::VectorXf::Zero(inputSize)), bias_(0.0f) {
}

float Perceptron::sigmoid(float z) {
    return 1.0f / (1.0f + std::exp(-z));
}

float Perceptron::infer(const Eigen::VectorXf& input) const {
    // Dot product is highly optimized in Eigen (SIMD)
    float z = weights_.dot(input) + bias_;
    return sigmoid(z);
}

bool Perceptron::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ML] Error loading model: " << path << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;

        // Deserialize standard C++ vector -> Eigen Vector
        std::vector<float> w = j["weights"].get<std::vector<float>>();
        bias_ = j["bias"].get<float>();

        if (w.size() != static_cast<size_t>(weights_.size())) {
            std::cerr << "[ML] Dimension mismatch. Json: " << w.size() 
                      << ", Alloc: " << weights_.size() << std::endl;
            // Resize if we want to be flexible, but better to warn
            weights_.resize(static_cast<Eigen::Index>(w.size()));
        }

        // Copy data
        for (size_t i = 0; i < w.size(); ++i) {
            weights_[static_cast<Eigen::Index>(i)] = w[i];
        }

        std::cout << "[ML] Model loaded successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ML] JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

    float Perceptron::sigmoidPrime(float z) {
        float s = sigmoid(z);
        return s * (1.0f - s);
    }

    void Perceptron::train(const Eigen::VectorXf& input, float target, float lr) {
        // Forward
        float z = weights_.dot(input) + bias_;
        float prediction = sigmoid(z);

        // Error (Local Gradient)
        float error = target - prediction; // dLoss/dPred * dPred/dZ (simplified MSE derivative)
        
        // Use derivative of sigmoid for correct backprop
        float delta = error * sigmoidPrime(z);

        // Update Weights: w += lr * input * delta
        weights_ += lr * delta * input;

        // Update Bias: b += lr * delta
        bias_ += lr * delta;
    }

} // namespace ml
