#include "shader.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace graphics {

namespace {
    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open shader file: " + filename);
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
        file.close();

        return buffer;
    }
}

Shader::Shader(const core::GraphicsContext& context, const std::string& filepath) 
    : device_(context.device()), filepath_(filepath) {
    
    // Try to load from multiple paths
    std::vector<std::string> paths = {
        filepath,
        "build/" + filepath,
        "../" + filepath, // If running from build dir
        "../build/" + filepath
    };

    std::vector<char> code;
    bool found = false;
    std::string loadedPath;

    for (const auto& path : paths) {
        try {
            code = readFile(path);
            found = true;
            loadedPath = path;
            break;
        } catch (...) {
            continue;
        }
    }

    if (!found) {
        throw std::runtime_error("Failed to open shader file. Tried: " + filepath + ", build/" + filepath);
    }

    std::cout << "Loaded shader: " << loadedPath << std::endl;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module for: " + filepath);
    }
}

Shader::~Shader() {
    if (module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, module_, nullptr);
    }
}

Shader::Shader(Shader&& other) noexcept 
    : device_(other.device_), module_(other.module_), filepath_(std::move(other.filepath_)) {
    other.module_ = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module_, nullptr);
        }
        device_ = other.device_;
        module_ = other.module_;
        filepath_ = std::move(other.filepath_);
        other.module_ = VK_NULL_HANDLE;
    }
    return *this;
}

VkShaderStageFlagBits Shader::stage() const {
    if (filepath_.find(".vert") != std::string::npos) return VK_SHADER_STAGE_VERTEX_BIT;
    if (filepath_.find(".frag") != std::string::npos) return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (filepath_.find(".comp") != std::string::npos) return VK_SHADER_STAGE_COMPUTE_BIT;
    return VK_SHADER_STAGE_ALL;
}

} // namespace graphics
