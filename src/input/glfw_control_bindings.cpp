#include "input/glfw_control_bindings.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <GLFW/glfw3.h>
#include <fmt/format.h>

namespace vac
{
namespace
{
std::string normalizedKeyName(std::string_view name)
{
    std::string normalized;
    normalized.reserve(name.size());
    for (const char c : name) {
        if (c == '-' || c == ' ') {
            normalized.push_back('_');
        } else {
            normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return normalized;
}
} // namespace

std::optional<int> glfwKeyCodeFromName(std::string_view name)
{
    const std::string key = normalizedKeyName(name);
    if (key.size() == 1) {
        const char c = key.front();
        if (c >= 'A' && c <= 'Z') {
            return GLFW_KEY_A + (c - 'A');
        }
        if (c >= '0' && c <= '9') {
            return GLFW_KEY_0 + (c - '0');
        }
    }

    static const std::unordered_map<std::string, int> kNamedKeys = {
        {"ESC", GLFW_KEY_ESCAPE},
        {"ESCAPE", GLFW_KEY_ESCAPE},
        {"TAB", GLFW_KEY_TAB},
        {"CAPS", GLFW_KEY_CAPS_LOCK},
        {"CAPS_LOCK", GLFW_KEY_CAPS_LOCK},
        {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT},
        {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
        {"SHIFT", GLFW_KEY_LEFT_SHIFT},
        {"LEFT", GLFW_KEY_LEFT},
        {"RIGHT", GLFW_KEY_RIGHT},
        {"UP", GLFW_KEY_UP},
        {"DOWN", GLFW_KEY_DOWN},
        {"SPACE", GLFW_KEY_SPACE},
        {"ENTER", GLFW_KEY_ENTER},
        {"RETURN", GLFW_KEY_ENTER},
        {"LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL},
        {"RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL},
        {"LEFT_ALT", GLFW_KEY_LEFT_ALT},
        {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT},
    };

    const auto it = kNamedKeys.find(key);
    if (it == kNamedKeys.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<int> resolveKeyList(const std::vector<std::string> &names, std::string_view actionName)
{
    std::vector<int> keys;
    keys.reserve(names.size());
    for (const std::string &name : names) {
        const std::optional<int> keyCode = glfwKeyCodeFromName(name);
        if (!keyCode.has_value()) {
            throw std::runtime_error(fmt::format("Unknown key '{}' in action '{}'", name, actionName));
        }
        keys.push_back(*keyCode);
    }
    return keys;
}

ResolvedControlBindings resolveControlBindings(const ControlBindings &bindings)
{
    return {
        resolveKeyList(bindings.closeViewer, "close_viewer"),
        resolveKeyList(bindings.cameraModeToggle, "camera_mode_toggle"),
        resolveKeyList(bindings.playerMoveLeft, "player_move_left"),
        resolveKeyList(bindings.playerMoveRight, "player_move_right"),
        resolveKeyList(bindings.playerMoveForward, "player_move_forward"),
        resolveKeyList(bindings.playerMoveBackward, "player_move_backward"),
        resolveKeyList(bindings.playerSprint, "player_sprint"),
        resolveKeyList(bindings.sparringMoveLeft, "sparring_move_left"),
        resolveKeyList(bindings.sparringMoveRight, "sparring_move_right"),
        resolveKeyList(bindings.sparringMoveForward, "sparring_move_forward"),
        resolveKeyList(bindings.sparringMoveBackward, "sparring_move_backward"),
    };
}
} // namespace vac
