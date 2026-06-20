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
    src/combat/combat_simulation.cpp
    src/render/scene_geometry.cpp
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

function(add_shader TARGET_NAME SOURCE_FILE)
    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${TARGET_NAME}.spv")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND Vulkan::glslc "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}" -o "${OUTPUT_FILE}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}"
        VERBATIM
    )
    add_custom_target(${TARGET_NAME}_shader DEPENDS "${OUTPUT_FILE}")
    set(${TARGET_NAME}_SPV "${OUTPUT_FILE}" PARENT_SCOPE)
endfunction()

add_shader(scene_vert shaders/scene.vert)
add_shader(scene_frag shaders/scene.frag)

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

add_executable(vulkan_scene_viewer
    src/vulkan_scene_viewer.cpp
)

add_dependencies(vulkan_scene_viewer
    scene_vert_shader
    scene_frag_shader
)

target_compile_features(vulkan_scene_viewer PRIVATE cxx_std_20)

target_compile_definitions(vulkan_scene_viewer PRIVATE
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    VULKAN_ACTION_CLIENT_SCENE_VERT_SPV="${scene_vert_SPV}"
    VULKAN_ACTION_CLIENT_SCENE_FRAG_SPV="${scene_frag_SPV}"
)

target_link_libraries(vulkan_scene_viewer PRIVATE
    scene_core
    Vulkan::Vulkan
    fmt::fmt
    glfw
    glm::glm
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(vulkan_scene_viewer PRIVATE Xinput.lib)
endif()
