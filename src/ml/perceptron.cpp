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
            weights_.resize(w.size());
        }

        // Copy data
        for (size_t i = 0; i < w.size(); ++i) {
            weights_[i] = w[i];
        }

        std::cout << "[ML] Model loaded successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ML] JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ml
