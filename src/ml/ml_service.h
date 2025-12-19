#pragma once
#include "perceptron.h"
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <Eigen/Dense>

namespace ml {

class MLService {
public:
    MLService();
    ~MLService();

    // Initialize the service (e.g., load config)
    // Initialize the service
    void init();

    // Load a model from disk
    bool loadModel(const std::string& name, const std::string& path, int inputSize = 4);

    // Generic Prediction
    // Returns scalar output [0-1]
    float predict(const std::string& modelName, const Eigen::VectorXf& inputs) const;

    // Helper: Soil Color specific wrapper (keeps existing UI code working)
    Eigen::Vector3f predictSoilColor(float n, float p, float k, float ph) const;

    // Generic Training
    void collectTrainingSample(const std::string& modelName, const std::vector<float>& inputs, float target);
    size_t datasetSize(const std::string& modelName) const;
    void trainModel(const std::string& modelName, int epochs = 50, float learningRate = 0.05f);

private:
    struct GenericSample {
        std::vector<float> inputs; 
        float target; 
    };
    
    // Data per model
    std::map<std::string, std::vector<GenericSample>> trainingSets_;
    std::map<std::string, std::unique_ptr<Perceptron>> models_;
};

} // namespace ml
