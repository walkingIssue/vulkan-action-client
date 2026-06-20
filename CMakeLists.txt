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

add_library(content_core
    src/content/authoring_scene.cpp
    src/content/move_asset.cpp
)

target_compile_features(content_core PUBLIC cxx_std_20)

target_include_directories(content_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(content_core PUBLIC
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(content_core PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
)

add_library(animation_core
    src/animation/proxy_animation.cpp
)

target_compile_features(animation_core PUBLIC cxx_std_20)

target_include_directories(animation_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(animation_core PUBLIC
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(animation_core PUBLIC
    content_core
    glm::glm
    nlohmann_json::nlohmann_json
)

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

add_library(simulation_core
    src/simulation/runtime_world.cpp
)

target_compile_features(simulation_core PUBLIC cxx_std_20)

target_include_directories(simulation_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(simulation_core PUBLIC
    content_core
    scene_core
)

add_library(combat_runtime_core
    src/combat/combat_collision.cpp
    src/combat/combat_runtime.cpp
)

target_compile_features(combat_runtime_core PUBLIC cxx_std_20)

target_include_directories(combat_runtime_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(combat_runtime_core PUBLIC
    content_core
    simulation_core
)

add_library(map_command_core
    src/authoring/map_command_script.cpp
)

target_compile_features(map_command_core PUBLIC cxx_std_20)

target_include_directories(map_command_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(map_command_core PUBLIC
    content_core
    simulation_core
    nlohmann_json::nlohmann_json
)

add_library(visual_lab_core
    src/visual_lab/visual_lab.cpp
)

target_compile_features(visual_lab_core PUBLIC cxx_std_20)

target_include_directories(visual_lab_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(visual_lab_core PUBLIC
    animation_core
    combat_runtime_core
    content_core
    scene_core
    simulation_core
)

add_library(combat_scenario_core
    src/combat/combat_scenario.cpp
)

target_compile_features(combat_scenario_core PUBLIC cxx_std_20)

target_include_directories(combat_scenario_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(combat_scenario_core PUBLIC
    VULKAN_ACTION_CLIENT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(combat_scenario_core PUBLIC
    animation_core
    combat_runtime_core
    nlohmann_json::nlohmann_json
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

add_library(host_core
    src/host/host_cli.cpp
)

target_compile_features(host_core PUBLIC cxx_std_20)

target_include_directories(host_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(host_core PUBLIC
    fmt::fmt
    nlohmann_json::nlohmann_json
)

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
    host_core
)

add_executable(map_command_runner
    src/map_command_runner.cpp
)

target_compile_features(map_command_runner PRIVATE cxx_std_20)

target_link_libraries(map_command_runner PRIVATE
    map_command_core
    host_core
)

add_executable(udp_state_server
    src/udp_state_server.cpp
)

target_compile_features(udp_state_server PRIVATE cxx_std_20)

target_link_libraries(udp_state_server PRIVATE
    network_core
)

add_executable(combat_scenario_runner
    src/combat_scenario_runner.cpp
)

target_compile_features(combat_scenario_runner PRIVATE cxx_std_20)

target_link_libraries(combat_scenario_runner PRIVATE
    combat_scenario_core
    host_core
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
    host_core
    input_core
    network_core
    visual_lab_core
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
    add_executable(authoring_scene_tests
        tests/authoring_scene_tests.cpp
    )

    target_compile_features(authoring_scene_tests PRIVATE cxx_std_20)

    target_link_libraries(authoring_scene_tests PRIVATE
        content_core
    )

    add_test(NAME authoring_scene_tests COMMAND authoring_scene_tests)
    set_tests_properties(authoring_scene_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;content;authoring;map"
    )

    add_executable(move_asset_tests
        tests/move_asset_tests.cpp
    )

    target_compile_features(move_asset_tests PRIVATE cxx_std_20)

    target_link_libraries(move_asset_tests PRIVATE
        content_core
    )

    add_test(NAME move_asset_tests COMMAND move_asset_tests)
    set_tests_properties(move_asset_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;content;combat;move"
    )

    add_executable(simulation_runtime_tests
        tests/simulation_runtime_tests.cpp
    )

    target_compile_features(simulation_runtime_tests PRIVATE cxx_std_20)

    target_link_libraries(simulation_runtime_tests PRIVATE
        simulation_core
    )

    add_test(NAME simulation_runtime_tests COMMAND simulation_runtime_tests)
    set_tests_properties(simulation_runtime_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;simulation;runtime"
    )

    add_executable(combat_runtime_tests
        tests/combat_runtime_tests.cpp
    )

    target_compile_features(combat_runtime_tests PRIVATE cxx_std_20)

    target_link_libraries(combat_runtime_tests PRIVATE
        combat_runtime_core
    )

    add_test(NAME combat_runtime_tests COMMAND combat_runtime_tests)
    set_tests_properties(combat_runtime_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;combat;runtime"
    )

    add_executable(combat_collision_tests
        tests/combat_collision_tests.cpp
    )

    target_compile_features(combat_collision_tests PRIVATE cxx_std_20)

    target_link_libraries(combat_collision_tests PRIVATE
        combat_runtime_core
    )

    add_test(NAME combat_collision_tests COMMAND combat_collision_tests)
    set_tests_properties(combat_collision_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;combat;collision"
    )

    add_executable(proxy_animation_tests
        tests/proxy_animation_tests.cpp
    )

    target_compile_features(proxy_animation_tests PRIVATE cxx_std_20)

    target_link_libraries(proxy_animation_tests PRIVATE
        animation_core
    )

    add_test(NAME proxy_animation_tests COMMAND proxy_animation_tests)
    set_tests_properties(proxy_animation_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;animation;combat"
    )

    add_executable(map_command_runner_tests
        tests/map_command_runner_tests.cpp
    )

    target_compile_features(map_command_runner_tests PRIVATE cxx_std_20)

    target_link_libraries(map_command_runner_tests PRIVATE
        map_command_core
    )

    add_test(NAME map_command_runner_tests COMMAND map_command_runner_tests)
    set_tests_properties(map_command_runner_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;content;authoring;map"
    )

    add_executable(visual_lab_tests
        tests/visual_lab_tests.cpp
    )

    target_compile_features(visual_lab_tests PRIVATE cxx_std_20)

    target_link_libraries(visual_lab_tests PRIVATE
        visual_lab_core
    )

    add_test(NAME visual_lab_tests COMMAND visual_lab_tests)
    set_tests_properties(visual_lab_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;viewer;visual;combat;map"
    )

    add_test(
        NAME map_command_runner_process
        COMMAND map_command_runner
                --command-script "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/commands/wire_arena.commands.json"
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/map_command_runner_result.json"
                --headless
    )
    set_tests_properties(map_command_runner_process PROPERTIES
        TIMEOUT 20
        LABELS "content;authoring;map;process"
    )

    add_executable(combat_scenario_tests
        tests/combat_scenario_tests.cpp
    )

    target_compile_features(combat_scenario_tests PRIVATE cxx_std_20)

    target_link_libraries(combat_scenario_tests PRIVATE
        combat_scenario_core
    )

    add_test(NAME combat_scenario_tests COMMAND combat_scenario_tests)
    set_tests_properties(combat_scenario_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;integration;combat;scenario"
    )

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

    add_executable(host_cli_tests
        tests/host_cli_tests.cpp
    )

    target_compile_features(host_cli_tests PRIVATE cxx_std_20)

    target_link_libraries(host_cli_tests PRIVATE
        host_core
    )

    add_test(NAME host_cli_tests COMMAND host_cli_tests)
    set_tests_properties(host_cli_tests PROPERTIES
        TIMEOUT 10
        LABELS "unit;host"
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

    add_executable(characterization_tests
        tests/characterization_tests.cpp
    )

    target_compile_features(characterization_tests PRIVATE cxx_std_20)

    target_link_libraries(characterization_tests PRIVATE
        scene_core
        network_core
    )

    add_test(NAME characterization_tests COMMAND characterization_tests)
    set_tests_properties(characterization_tests PROPERTIES
        TIMEOUT 20
        LABELS "unit;characterization;scene;combat;movement;network;protocol;relay"
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

    add_test(
        NAME viewer_smoke
        COMMAND vulkan_scene_viewer
                --frames 3
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/viewer_smoke_result.json"
    )
    set_tests_properties(viewer_smoke PROPERTIES
        TIMEOUT 30
        LABELS "viewer;smoke;characterization"
    )

    add_test(
        NAME visual_lab_smoke
        COMMAND vulkan_scene_viewer
                --visual-lab
                --offline
                --scene "${CMAKE_CURRENT_SOURCE_DIR}/content/maps/basic_primitive_arena.map.json"
                --frames 3
                --ticks 20
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/visual_lab_smoke_result.json"
    )
    set_tests_properties(visual_lab_smoke PROPERTIES
        TIMEOUT 30
        LABELS "viewer;smoke;visual;process"
    )

    add_test(
        NAME combat_scenario_runner_hits
        COMMAND combat_scenario_runner
                --scenario "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json"
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/scenario_hits_result.json"
                --headless
    )
    set_tests_properties(combat_scenario_runner_hits PROPERTIES
        TIMEOUT 10
        LABELS "combat;scenario;process"
    )

    add_test(
        NAME combat_scenario_runner_whiff
        COMMAND combat_scenario_runner
                --scenario "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scenarios/sword_light_whiffs.scenario.json"
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/scenario_whiff_result.json"
                --headless
    )
    set_tests_properties(combat_scenario_runner_whiff PROPERTIES
        TIMEOUT 10
        LABELS "combat;scenario;process"
    )

    add_test(
        NAME combat_scenario_runner_cancel_on_hit
        COMMAND combat_scenario_runner
                --scenario "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scenarios/sword_light_cancel_on_hit.scenario.json"
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/scenario_cancel_on_hit_result.json"
                --headless
    )
    set_tests_properties(combat_scenario_runner_cancel_on_hit PROPERTIES
        TIMEOUT 10
        LABELS "combat;scenario;process"
    )

    add_test(
        NAME combat_scenario_runner_dodge_invulnerability
        COMMAND combat_scenario_runner
                --scenario "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scenarios/dodge_invulnerability_boundary.scenario.json"
                --result-file "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/scenario_dodge_invulnerability_result.json"
                --headless
    )
    set_tests_properties(combat_scenario_runner_dodge_invulnerability PROPERTIES
        TIMEOUT 10
        LABELS "combat;scenario;process"
    )

    add_test(
        NAME combat_scenario_runner_update_guard
        COMMAND combat_scenario_runner
                --scenario "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json"
                --update-golden
                --headless
    )
    set_tests_properties(combat_scenario_runner_update_guard PROPERTIES
        TIMEOUT 10
        WILL_FAIL TRUE
        LABELS "combat;scenario;process"
    )
endif()
