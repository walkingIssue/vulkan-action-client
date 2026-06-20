#include <assimp/Importer.hpp>
#include <fmt/core.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
    uint32_t apiVersion = 0;
    const VkResult versionResult = vkEnumerateInstanceVersion(&apiVersion);
    if (versionResult != VK_SUCCESS) {
        std::cerr << "vkEnumerateInstanceVersion failed: " << versionResult << "\n";
        return 1;
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "glfwInit failed\n";
        return 2;
    }
    glfwTerminate();

    Assimp::Importer importer;
    std::string extensions;
    importer.GetExtensionList(extensions);

    const glm::vec3 basis{1.0f, 0.0f, 0.0f};
    const nlohmann::json summary = {
        {"vulkan_major", VK_VERSION_MAJOR(apiVersion)},
        {"vulkan_minor", VK_VERSION_MINOR(apiVersion)},
        {"vulkan_patch", VK_VERSION_PATCH(apiVersion)},
        {"basis_x", basis.x},
        {"assimp_extensions_size", extensions.size()}
    };

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::DestroyContext();

    spdlog::info("Toolchain probe linked: {}", summary.dump());
    std::cout << fmt::format("Vulkan {}.{}.{} ready\n",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));

    constexpr int stbVersion = STBI_VERSION;
    (void)stbVersion;
    (void)sizeof(VmaAllocator);
    return 0;
}
