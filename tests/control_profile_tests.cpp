#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>

#include "config/control_profile.hpp"
#include "input/glfw_control_bindings.hpp"

namespace
{
int g_failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    expect(std::fabs(actual - expected) <= tolerance, message);
}

template <typename Fn>
void expectThrows(Fn &&fn, std::string_view message)
{
    try {
        fn();
        expect(false, message);
    } catch (const std::exception &) {
        expect(true, message);
    }
}

std::filesystem::path writeTempProfile(std::string_view contents)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("vac_control_profile_test_" + std::to_string(now) + ".json");
    std::ofstream file(path);
    file << contents;
    file.close();
    return path;
}

bool equals(const std::vector<std::string> &actual, std::initializer_list<std::string_view> expected)
{
    if (actual.size() != expected.size()) {
        return false;
    }

    size_t index = 0;
    for (const std::string_view value : expected) {
        if (actual[index] != value) {
            return false;
        }
        ++index;
    }
    return true;
}

bool equals(const std::vector<int> &actual, std::initializer_list<int> expected)
{
    if (actual.size() != expected.size()) {
        return false;
    }

    size_t index = 0;
    for (const int value : expected) {
        if (actual[index] != value) {
            return false;
        }
        ++index;
    }
    return true;
}
} // namespace

int main()
{
    vac::ControlProfile checkedInProfile;
    try {
        checkedInProfile = vac::loadControlProfile(vac::defaultControlProfilePath());
    } catch (const std::exception &error) {
        std::cerr << "FAIL: checked-in profile loads from "
                  << vac::defaultControlProfilePath().string()
                  << ": " << error.what() << '\n';
        return 1;
    }

    expectNear(checkedInProfile.movement.playerRunSpeedWorldUnitsPerSecond,
               37.7f,
               0.001f,
               "checked-in profile keeps current player run speed");
    expectNear(checkedInProfile.movement.playerSprintSpeedScale,
               2.3f,
               0.001f,
               "checked-in profile keeps current sprint scale");
    expectNear(checkedInProfile.movement.backpedalSpeedScale,
               1.0f / 3.0f,
               0.001f,
               "checked-in profile keeps current backpedal scale");
    expect(equals(checkedInProfile.bindings.cameraModeToggle, {"TAB", "CAPS_LOCK"}),
           "checked-in profile keeps camera toggle bindings");
    expect(equals(checkedInProfile.bindings.playerMoveForward, {"W"}),
           "checked-in profile keeps forward binding");

    const vac::ResolvedControlBindings resolvedCheckedIn =
        vac::resolveControlBindings(checkedInProfile.bindings);
    expect(equals(resolvedCheckedIn.closeViewer, {GLFW_KEY_ESCAPE}),
           "checked-in close binding resolves");
    expect(equals(resolvedCheckedIn.cameraModeToggle, {GLFW_KEY_TAB, GLFW_KEY_CAPS_LOCK}),
           "checked-in camera toggles resolve");
    expect(equals(resolvedCheckedIn.playerSprint, {GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT}),
           "checked-in sprint bindings resolve");

    expect(vac::glfwKeyCodeFromName("caps-lock").value_or(-1) == GLFW_KEY_CAPS_LOCK,
           "key resolver accepts hyphenated names");
    expect(vac::glfwKeyCodeFromName("left shift").value_or(-1) == GLFW_KEY_LEFT_SHIFT,
           "key resolver accepts spaced names");
    expect(vac::glfwKeyCodeFromName("q").value_or(-1) == GLFW_KEY_Q,
           "key resolver accepts lowercase letters");
    expect(!vac::glfwKeyCodeFromName("NOT_A_REAL_KEY").has_value(),
           "key resolver rejects unknown names");

    const std::filesystem::path fallbackPath = writeTempProfile(R"json(
{
  "movement": {
    "player_run_speed_world_units_per_second": "fast",
    "player_sprint_speed_scale": 1.6,
    "backpedal_speed_scale": -2.0,
    "move_target_arrival_radius_world_units": 2.5
  },
  "bindings": {
    "close_viewer": [],
    "player_move_left": [42],
    "player_move_right": "L",
    "player_sprint": ["LEFT_SHIFT", 12, "RIGHT_SHIFT"]
  }
}
)json");

    const vac::ControlProfile fallbackProfile = vac::loadControlProfile(fallbackPath);
    std::filesystem::remove(fallbackPath);
    expectNear(fallbackProfile.movement.playerRunSpeedWorldUnitsPerSecond,
               vac::combat::kDefaultMovementTuning.playerRunSpeedWorldUnitsPerSecond,
               0.001f,
               "non-numeric movement value falls back");
    expectNear(fallbackProfile.movement.playerSprintSpeedScale,
               1.6f,
               0.001f,
               "numeric movement override loads");
    expectNear(fallbackProfile.movement.backpedalSpeedScale,
               0.0f,
               0.001f,
               "negative movement value clamps to minimum");
    expectNear(fallbackProfile.movement.moveTargetArrivalRadiusWorldUnits,
               2.5f,
               0.001f,
               "arrival radius override loads");
    expect(equals(fallbackProfile.bindings.closeViewer, {"ESCAPE"}),
           "empty binding list falls back");
    expect(equals(fallbackProfile.bindings.playerMoveLeft, {"A"}),
           "binding list with no strings falls back");
    expect(equals(fallbackProfile.bindings.playerMoveRight, {"L"}),
           "single string binding loads");
    expect(equals(fallbackProfile.bindings.playerSprint, {"LEFT_SHIFT", "RIGHT_SHIFT"}),
           "binding list filters non-string entries");

    vac::ControlBindings badBindings;
    badBindings.playerMoveForward = {"NOT_A_REAL_KEY"};
    expectThrows([&]() { (void)vac::resolveControlBindings(badBindings); },
                 "unknown binding key is rejected during resolution");

    const std::filesystem::path invalidJsonPath = writeTempProfile("{ this is not valid json");
    expectThrows([&]() { (void)vac::loadControlProfile(invalidJsonPath); },
                 "invalid JSON profile throws");
    std::filesystem::remove(invalidJsonPath);

    expectThrows([&]() { (void)vac::loadControlProfile("missing-control-profile.json"); },
                 "missing profile file throws");

    return g_failures == 0 ? 0 : 1;
}
