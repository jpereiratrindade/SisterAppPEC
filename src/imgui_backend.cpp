#include "imgui_backend.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <array>
#include <cstdio>

#include "core/vk_utils.h"

bool initImGuiCore(SDLContext& sdl, core::GraphicsContext& ctx, VkDescriptorPool& descriptorPool) {
    if (ImGui::GetCurrentContext() == nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
    }

    static bool sdlInitialized = false;
    if (!sdlInitialized) {
        if (!ImGui_ImplSDL2_InitForVulkan(sdl.window)) {
            std::fprintf(stderr, "Falha ao inicializar backend SDL2 do ImGui.\n");
            return false;
        }
        sdlInitialized = true;
    }

    if (descriptorPool == VK_NULL_HANDLE) {
        std::array<VkDescriptorPoolSize, 4> poolSizes = {{
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 }
        }};

        VkDescriptorPoolCreateInfo dpci{};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpci.maxSets = 100 * static_cast<uint32_t>(poolSizes.size());
        dpci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        dpci.pPoolSizes = poolSizes.data();

        core::check_vk(vkCreateDescriptorPool(ctx.device(), &dpci, nullptr, &descriptorPool));
    }

    return true;
}

static bool g_imguiVulkanInitialized = false;

bool initImGuiVulkan(core::GraphicsContext& ctx, VkRenderPass renderPass, uint32_t imageCount, VkDescriptorPool descriptorPool) {
    if (descriptorPool == VK_NULL_HANDLE) {
        std::fprintf(stderr, "Descriptor pool nao inicializado.\n");
        return false;
    }

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = ctx.instance();
    info.PhysicalDevice = ctx.physicalDevice();
    info.Device = ctx.device();
    info.QueueFamily = ctx.queueFamilyIndex();
    info.Queue = ctx.graphicsQueue();
    info.DescriptorPool = descriptorPool;
    info.RenderPass = renderPass;
    info.MinImageCount = imageCount;
    info.ImageCount = imageCount;
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.CheckVkResultFn = core::check_vk;

    if (!ImGui_ImplVulkan_Init(&info)) {
        std::fprintf(stderr, "Falha ao inicializar backend Vulkan do ImGui.\n");
        return false;
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        std::fprintf(stderr, "Falha ao criar fontes do ImGui.\n");
        return false;
    }

    g_imguiVulkanInitialized = true;
    return true;
}

void shutdownImGuiVulkanIfNeeded() {
    if (ImGui::GetCurrentContext() && g_imguiVulkanInitialized) {
        ImGui_ImplVulkan_Shutdown();
        g_imguiVulkanInitialized = false;
    }
}

void beginGuiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void endGuiFrame() {
    ImGui::Render();
}

void drawDebugGui(float dtSeconds, std::size_t entityCount) {
    ImGui::Begin("Debug Info");
    ImGui::Text("FPS: %.1f (dt=%.3f ms)", ImGui::GetIO().Framerate, dtSeconds * 1000.0f);
    ImGui::Text("Entidades: %zu", entityCount);
    ImGui::End();
}
