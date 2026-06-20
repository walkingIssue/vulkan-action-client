#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "combat/combat_simulation.hpp"

namespace vac
{
struct ControlBindings
{
    std::vector<std::string> closeViewer{"ESCAPE"};
    std::vector<std::string> cameraModeToggle{"TAB", "CAPS_LOCK"};
    std::vector<std::string> playerMoveLeft{"A"};
    std::vector<std::string> playerMoveRight{"D"};
    std::vector<std::string> playerMoveForward{"W"};
    std::vector<std::string> playerMoveBackward{"S"};
    std::vector<std::string> playerSprint{"LEFT_SHIFT", "RIGHT_SHIFT"};
    std::vector<std::string> sparringMoveLeft{"LEFT"};
    std::vector<std::string> sparringMoveRight{"RIGHT"};
    std::vector<std::string> sparringMoveForward{"UP"};
    std::vector<std::string> sparringMoveBackward{"DOWN"};
};

struct ControlProfile
{
    combat::MovementTuning movement;
    ControlBindings bindings;
};

std::filesystem::path defaultControlProfilePath();
ControlProfile loadControlProfile(const std::filesystem::path &path);
} // namespace vac
