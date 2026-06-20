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

add_library(scene_core
    src/scene/scene_runtime.cpp
)

target_compile_features(scene_core PUBLIC cxx_std_20)

target_include_directories(scene_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(scene_core PUBLIC
    assimp::assimp
    fmt::fmt
    glm::glm
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

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

add_executable(scene_probe
    src/scene_probe.cpp
)

target_compile_features(scene_probe PRIVATE cxx_std_20)

target_compile_definitions(scene_probe PRIVATE
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(scene_probe PRIVATE
    scene_core
)
