#include "ml_service.h"
#include <iostream>

namespace ml {

MLService::MLService() {}
MLService::~MLService() {}

void MLService::init() {
    std::cout << "[ML] Service Initialized." << std::endl;
}

    // Generic Load Model
    bool MLService::loadModel(const std::string& name, const std::string& path, int inputSize) {
        auto model = std::make_unique<Perceptron>(inputSize); 
        if (model->load(path)) {
            models_[name] = std::move(model);
            std::cout << "[ML] Loaded model: " << name << std::endl;
            return true;
        }
        std::cerr << "[ML] Failed to load model: " << name << " from " << path << std::endl;
        return false;
    }

    // Generic Prediction
    float MLService::predict(const std::string& modelName, const Eigen::VectorXf& inputs) const {
        auto it = models_.find(modelName);
        if (it != models_.end()) {
            return it->second->infer(inputs);
        }
        return 0.0f; // Default if model missing
    }

    // Soil Color Wrapper (Maintains backward compatibility for UI)
    Eigen::Vector3f MLService::predictSoilColor(float n, float p, float k, float ph) const {
        Eigen::VectorXf input(4);
        input << n, p, k, ph;
        
        float output = predict("soil_color", input);
        
        // Map scalar [0,1] to Color Gradient
        return Eigen::Vector3f(1.0f - output, output, 0.2f); 
    }

    // Runoff Wrapper
    float MLService::predictRunoff(float rain, float infil, float biomass) const {
        Eigen::VectorXf input(3);
        // Normalize inputs to match training data collection (which was normalized /100 /100)
        // Note: Application::mlCollectHydroData normalized rain/100, effInfil/100.
        // So we must normalize here too if we want correct inference.
        // Assuming raw inputs are passed here.
        float nRain = rain / 100.0f;
        float nInfil = infil / 100.0f;
        float nBio = biomass; // Already 0-1
        
        input << nRain, nInfil, nBio;
        
        float output = predict("hydro_runoff", input);
        // Output is normalized runoff/100.0f
        return output * 100.0f; // Denormalize
    }

    // Fire Risk Wrapper
    float MLService::predictFireRisk(float cEI, float cES, float vEI, float vES) const {
         Eigen::VectorXf input(4);
         input << cEI, cES, vEI, vES; // All 0-1
         return predict("fire_risk", input);
    }

    // Growth Wrapper
    float MLService::predictGrowth(float currentC, float k, float vigor) const {
         Eigen::VectorXf input(3);
         input << currentC, k, vigor; // All 0-1
         return predict("biomass_growth", input);
    }

    // Generic Collection
    void MLService::collectTrainingSample(const std::string& modelName, const std::vector<float>& inputs, float target) {
        trainingSets_[modelName].push_back({inputs, target});
    }

    // Generic Dataset Size
    size_t MLService::datasetSize(const std::string& modelName) const {
        auto it = trainingSets_.find(modelName);
        if (it != trainingSets_.end()) {
            return it->second.size();
        }
        return 0;
    }

    // Generic Training Loop
    void MLService::trainModel(const std::string& modelName, int epochs, float learningRate) {
        auto& data = trainingSets_[modelName];
        if (data.empty()) {
             std::cout << "[ML] No training data for model: " << modelName << std::endl;
             return;
        }

        // Auto-create model if missing (infer input size from data)
        if (models_.find(modelName) == models_.end()) {
            int inputSize = data[0].inputs.size();
            models_[modelName] = std::make_unique<Perceptron>(inputSize);
            std::cout << "[ML] Created new '" << modelName << "' model with " << inputSize << " inputs." << std::endl;
        }

        Perceptron* model = models_[modelName].get();
        std::cout << "[ML] Training '" << modelName << "' started. Epochs: " << epochs << ", Samples: " << data.size() << std::endl;
        
        for (int e = 0; e < epochs; ++e) {
            for (const auto& sample : data) {
                 Eigen::VectorXf input(sample.inputs.size());
                 for(size_t i=0; i<sample.inputs.size(); ++i) input(i) = sample.inputs[i];
                 
                 model->train(input, sample.target, learningRate);
            }
        }
        std::cout << "[ML] Training '" << modelName << "' finished." << std::endl;
    }

} // namespace ml
