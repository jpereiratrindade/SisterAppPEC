#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct ShaderModule {
    VkShaderModule handle = VK_NULL_HANDLE;
    std::string path;
};

// Carrega um módulo de shader SPIR-V a partir de arquivo .spv.
// Retorna handle válido ou VK_NULL_HANDLE em caso de erro (com log).
ShaderModule loadShaderModule(VkDevice device, const std::string& path);

// Destroi o módulo carregado, se existir.
void destroyShaderModule(VkDevice device, ShaderModule& shader);
