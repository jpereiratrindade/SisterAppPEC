#include "shader_loader.h"
#include "log.h"

#include <cstdio>
#include <fstream>
#include <vector>

static std::vector<uint32_t> readSpirV(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer((size + 3) / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));

    return buffer;
}

ShaderModule loadShaderModule(VkDevice device, const std::string& path) {
    ShaderModule shader{};
    shader.path = path;

    auto spirv = readSpirV(path);
    if (spirv.empty()) {
        logMessage(LogLevel::Warn, "[ShaderLoader] Erro ao ler %s\n", path.c_str());
        return shader;
    }

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();

    if (vkCreateShaderModule(device, &info, nullptr, &shader.handle) != VK_SUCCESS) {
        logMessage(LogLevel::Error, "[ShaderLoader] Falha ao criar shader module: %s\n", path.c_str());
        shader.handle = VK_NULL_HANDLE;
    }

    return shader;
}

void destroyShaderModule(VkDevice device, ShaderModule& shader) {
    if (shader.handle) {
        vkDestroyShaderModule(device, shader.handle, nullptr);
    }
    shader.handle = VK_NULL_HANDLE;
}
