#include "config/control_profile.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "scene/scene_runtime.hpp"

namespace vac
{
namespace
{
using json = nlohmann::json;

float readFloat(const json &object, const char *key, float fallback, float minimum)
{
    if (!object.is_object()) {
        return fallback;
    }

    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return fallback;
    }

    return std::max(it->get<float>(), minimum);
}

std::vector<std::string> readBindings(const json &object,
                                      const char *key,
                                      const std::vector<std::string> &fallback)
{
    if (!object.is_object()) {
        return fallback;
    }

    const auto it = object.find(key);
    if (it == object.end()) {
        return fallback;
    }

    std::vector<std::string> bindings;
    if (it->is_string()) {
        bindings.push_back(it->get<std::string>());
    } else if (it->is_array()) {
        for (const json &value : *it) {
            if (value.is_string()) {
                bindings.push_back(value.get<std::string>());
            }
        }
    }

    return bindings.empty() ? fallback : bindings;
}
} // namespace

std::filesystem::path defaultControlProfilePath()
{
    return defaultProjectRoot() / "config/controls/player_control_profile.json";
}

ControlProfile loadControlProfile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error(fmt::format("Could not open control profile '{}'", path.string()));
    }

    json root;
    file >> root;

    ControlProfile profile;
    const json movement = root.value("movement", json::object());
    profile.movement.playerRunSpeedWorldUnitsPerSecond =
        readFloat(movement,
                  "player_run_speed_world_units_per_second",
                  profile.movement.playerRunSpeedWorldUnitsPerSecond,
                  0.0f);
    profile.movement.playerSprintSpeedScale =
        readFloat(movement, "player_sprint_speed_scale", profile.movement.playerSprintSpeedScale, 0.0f);
    profile.movement.backpedalSpeedScale =
        readFloat(movement, "backpedal_speed_scale", profile.movement.backpedalSpeedScale, 0.0f);
    profile.movement.moveTargetArrivalRadiusWorldUnits =
        readFloat(movement,
                  "move_target_arrival_radius_world_units",
                  profile.movement.moveTargetArrivalRadiusWorldUnits,
                  0.0f);
    profile.movement.sparringMoveSpeedWorldUnitsPerSecond =
        readFloat(movement,
                  "sparring_move_speed_world_units_per_second",
                  profile.movement.sparringMoveSpeedWorldUnitsPerSecond,
                  0.0f);
    profile.movement.turnSpeedDegreesPerSecond =
        readFloat(movement, "turn_speed_degrees_per_second", profile.movement.turnSpeedDegreesPerSecond, 0.0f);
    profile.movement.arenaEdgeInsetWorldUnits =
        readFloat(movement, "arena_edge_inset_world_units", profile.movement.arenaEdgeInsetWorldUnits, 0.0f);

    const json bindings = root.value("bindings", json::object());
    profile.bindings.closeViewer = readBindings(bindings, "close_viewer", profile.bindings.closeViewer);
    profile.bindings.cameraModeToggle = readBindings(bindings, "camera_mode_toggle", profile.bindings.cameraModeToggle);
    profile.bindings.playerMoveLeft = readBindings(bindings, "player_move_left", profile.bindings.playerMoveLeft);
    profile.bindings.playerMoveRight = readBindings(bindings, "player_move_right", profile.bindings.playerMoveRight);
    profile.bindings.playerMoveForward = readBindings(bindings, "player_move_forward", profile.bindings.playerMoveForward);
    profile.bindings.playerMoveBackward = readBindings(bindings, "player_move_backward", profile.bindings.playerMoveBackward);
    profile.bindings.playerSprint = readBindings(bindings, "player_sprint", profile.bindings.playerSprint);
    profile.bindings.sparringMoveLeft = readBindings(bindings, "sparring_move_left", profile.bindings.sparringMoveLeft);
    profile.bindings.sparringMoveRight = readBindings(bindings, "sparring_move_right", profile.bindings.sparringMoveRight);
    profile.bindings.sparringMoveForward =
        readBindings(bindings, "sparring_move_forward", profile.bindings.sparringMoveForward);
    profile.bindings.sparringMoveBackward =
        readBindings(bindings, "sparring_move_backward", profile.bindings.sparringMoveBackward);
    return profile;
}
} // namespace vac
