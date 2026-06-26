#include "viewer/viewer_options.hpp"

#include "config/control_profile.hpp"
#include "scene/scene_runtime.hpp"
#include "visual_lab/visual_lab.hpp"

#include <optional>
#include <stdexcept>

#include <fmt/format.h>

namespace vac::viewer
{
ViewerOptions parseOptions(int argc, char **argv)
{
    host::CommandLine commandLine{argc, argv};
    ViewerOptions options;
    options.common = host::parseCommonOptions(commandLine);
    host::rejectUnsupportedCommonOptions(
        options.common, {"--scene", "--frames", "--ticks", "--result-file", "--offline", "--hidden-window"});

    if (commandLine.consumeFlag("--visual-lab")) {
        options.visualLab = true;
    }

    const visual_lab::VisualLabAssetPaths labDefaults = visual_lab::defaultVisualLabAssetPaths();
    options.scenePath = options.visualLab
        ? labDefaults.mapPath
        : defaultProjectRoot() / "config/scenes/bootstrap.scene.json";
    options.controlProfilePath = defaultControlProfilePath();
    options.visualLabMovePath = labDefaults.movePath;
    options.visualLabProxyAnimationPath = labDefaults.proxyAnimationPath;

    if (options.common.scene.has_value()) {
        options.scenePath = *options.common.scene;
    }
    if (options.common.frames.has_value()) {
        options.frames = *options.common.frames;
    }

    if (const std::optional<std::string> controlProfile = commandLine.consumeValue("--control-profile")) {
        options.controlProfilePath = *controlProfile;
    }
    if (const std::optional<std::string> movePath = commandLine.consumeValue("--move")) {
        if (!options.visualLab) {
            throw host::ParseError("--move requires --visual-lab");
        }
        options.visualLabMovePath = *movePath;
    }
    if (const std::optional<std::string> animationPath = commandLine.consumeValue("--proxy-animation")) {
        if (!options.visualLab) {
            throw host::ParseError("--proxy-animation requires --visual-lab");
        }
        options.visualLabProxyAnimationPath = *animationPath;
    }
    if (const std::optional<std::string> scenarioPath = commandLine.consumeValue("--scenario")) {
        if (!options.visualLab) {
            throw host::ParseError("--scenario requires --visual-lab");
        }
        if (options.common.scene.has_value()) {
            throw host::ParseError("--scenario cannot be combined with --scene");
        }
        options.visualLabScenarioPath = *scenarioPath;
    }
    if (const std::optional<std::string> networkServer = commandLine.consumeValue("--net-server")) {
        options.networkServer = *networkServer;
    }
    if (const std::optional<std::string> clientIdValue = commandLine.consumeValue("--net-client-id")) {
        const int clientId = std::stoi(*clientIdValue);
        if (clientId < 0 || clientId > 255) {
            throw std::runtime_error("--net-client-id must be between 0 and 255");
        }
        options.networkClientId = static_cast<uint8_t>(clientId);
    }
    if (commandLine.consumeFlag("--orbit-camera")) {
        options.orbitCamera = true;
    }
    if (commandLine.consumeFlag("--static-camera")) {
        options.orbitCamera = false;
    }
    commandLine.rejectUnknown();

    return options;
}

void writeSuccessResultFile(const ViewerOptions &options,
                            const std::vector<std::string> &diagnostics)
{
    if (!options.common.resultFile.has_value()) {
        return;
    }

    host::HostResult result = host::resultFromOptions("vulkan_scene_viewer", options.common);
    result.message = options.visualLab
        ? fmt::format("Visual lab rendered {} frame(s)", options.frames)
        : options.frames > 0
        ? fmt::format("Rendered {} frame(s)", options.frames)
        : "Viewer exited normally";
    result.diagnostics = diagnostics;
    host::writeResultFile(*options.common.resultFile, result);
}

void writeErrorResultFile(const std::filesystem::path &resultFile,
                          const std::string &message)
{
    if (resultFile.empty()) {
        return;
    }

    host::HostResult result;
    result.host = "vulkan_scene_viewer";
    result.status = "error";
    result.message = message;
    host::writeResultFile(resultFile, result);
}
} // namespace vac::viewer
