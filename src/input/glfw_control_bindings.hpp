#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "config/control_profile.hpp"

namespace vac
{
struct ResolvedControlBindings
{
    std::vector<int> closeViewer;
    std::vector<int> cameraModeToggle;
    std::vector<int> playerMoveLeft;
    std::vector<int> playerMoveRight;
    std::vector<int> playerMoveForward;
    std::vector<int> playerMoveBackward;
    std::vector<int> playerSprint;
    std::vector<int> sparringMoveLeft;
    std::vector<int> sparringMoveRight;
    std::vector<int> sparringMoveForward;
    std::vector<int> sparringMoveBackward;
};

std::optional<int> glfwKeyCodeFromName(std::string_view name);
std::vector<int> resolveKeyList(const std::vector<std::string> &names, std::string_view actionName);
ResolvedControlBindings resolveControlBindings(const ControlBindings &bindings);
} // namespace vac
