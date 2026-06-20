cmake_minimum_required(VERSION 3.25)

project(vulkan_action_client LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

add_executable(toolchain_probe
    src/toolchain_probe.cpp
)

target_compile_features(toolchain_probe PRIVATE cxx_std_20)

target_include_directories(toolchain_probe PRIVATE
    ${Stb_INCLUDE_DIR}
)

target_link_libraries(toolchain_probe PRIVATE
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    Jolt::Jolt
    assimp::assimp
    fmt::fmt
    glfw
    glm::glm
    imgui::imgui
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
