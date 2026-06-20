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
    src/config/control_profile.cpp
    src/combat/combat_simulation.cpp
    src/render/scene_geometry.cpp
    src/scene/scene_runtime.cpp
)

target_compile_features(scene_core PUBLIC cxx_std_20)

target_include_directories(scene_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(scene_core PUBLIC
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(scene_core PUBLIC
    assimp::assimp
    fmt::fmt
    glm::glm
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

add_library(input_core
    src/input/glfw_control_bindings.cpp
)

target_compile_features(input_core PUBLIC cxx_std_20)

target_include_directories(input_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(input_core PUBLIC
    scene_core
    fmt::fmt
    glfw
)

add_library(network_core
    src/network/snapshot_client.cpp
    src/network/snapshot_relay.cpp
    src/network/state_protocol.cpp
    src/network/udp_socket.cpp
)

target_compile_features(network_core PUBLIC cxx_std_20)

target_include_directories(network_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(network_core PUBLIC
    fmt::fmt
    glm::glm
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(network_core PUBLIC Ws2_32)
endif()

include(CTest)

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

target_link_libraries(scene_probe PRIVATE
    scene_core
)

add_executable(udp_state_server
    src/udp_state_server.cpp
)

target_compile_features(udp_state_server PRIVATE cxx_std_20)

target_link_libraries(udp_state_server PRIVATE
    network_core
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
    VULKAN_ACTION_CLIENT_SCENE_VERT_SPV="${scene_vert_SPV}"
    VULKAN_ACTION_CLIENT_SCENE_FRAG_SPV="${scene_frag_SPV}"
)

target_link_libraries(vulkan_scene_viewer PRIVATE
    scene_core
    input_core
    network_core
    Vulkan::Vulkan
    fmt::fmt
    glfw
    glm::glm
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(vulkan_scene_viewer PRIVATE Xinput.lib)
endif()

if(BUILD_TESTING)
    add_executable(movement_behavior_tests
        tests/movement_behavior_tests.cpp
    )

    target_compile_features(movement_behavior_tests PRIVATE cxx_std_20)

    target_link_libraries(movement_behavior_tests PRIVATE
        scene_core
    )

    add_test(NAME movement_behavior_tests COMMAND movement_behavior_tests)
    set_tests_properties(movement_behavior_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;combat;movement"
    )

    add_executable(control_profile_tests
        tests/control_profile_tests.cpp
    )

    target_compile_features(control_profile_tests PRIVATE cxx_std_20)

    target_link_libraries(control_profile_tests PRIVATE
        input_core
    )

    add_test(NAME control_profile_tests COMMAND control_profile_tests)
    set_tests_properties(control_profile_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;config;input"
    )

    add_executable(network_protocol_tests
        tests/network_protocol_tests.cpp
    )

    target_compile_features(network_protocol_tests PRIVATE cxx_std_20)

    target_link_libraries(network_protocol_tests PRIVATE
        network_core
    )

    add_test(NAME network_protocol_tests COMMAND network_protocol_tests)
    set_tests_properties(network_protocol_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;network;protocol"
    )

    add_executable(snapshot_relay_tests
        tests/snapshot_relay_tests.cpp
    )

    target_compile_features(snapshot_relay_tests PRIVATE cxx_std_20)

    target_link_libraries(snapshot_relay_tests PRIVATE
        network_core
    )

    add_test(NAME snapshot_relay_tests COMMAND snapshot_relay_tests)
    set_tests_properties(snapshot_relay_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;network;relay"
    )

    add_executable(network_e2e_client
        tests/network_e2e_client.cpp
    )

    target_compile_features(network_e2e_client PRIVATE cxx_std_20)

    target_link_libraries(network_e2e_client PRIVATE
        network_core
    )

    add_test(
        NAME network_e2e
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-network-e2e.ps1"
                -BuildDir "$<TARGET_FILE_DIR:network_e2e_client>"
    )
    set_tests_properties(network_e2e PROPERTIES
        TIMEOUT 30
        LABELS "network;e2e"
    )
endif()
