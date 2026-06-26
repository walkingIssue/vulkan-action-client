#include "viewer/viewer_camera.hpp"
#include "viewer/viewer_options.hpp"

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        throw std::runtime_error(std::string{message});
    }
}

vac::viewer::ViewerOptions parse(std::vector<std::string> args)
{
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
        argv.push_back(arg.data());
    }
    return vac::viewer::parseOptions(static_cast<int>(argv.size()), argv.data());
}

void expectParseFailure(std::vector<std::string> args, std::string_view expectedMessage)
{
    try {
        (void)parse(std::move(args));
        expect(false, "parse failure expected");
    } catch (const std::exception &error) {
        expect(std::string_view{error.what()}.find(expectedMessage) != std::string_view::npos,
               expectedMessage);
    }
}

nlohmann::json readJson(const std::filesystem::path &path)
{
    std::ifstream input{path};
    expect(input.good(), "result file can be opened");
    return nlohmann::json::parse(input);
}

void parseStandardViewerOptions()
{
    const vac::viewer::ViewerOptions options =
        parse({"viewer",
               "--frames", "9",
               "--scene", "arena.scene.json",
               "--control-profile", "profile.controls.json",
               "--net-server", "127.0.0.1:41000",
               "--net-client-id", "37",
               "--orbit-camera"});

    expect(options.frames == 9, "frames are copied from common options");
    expect(options.common.frames.has_value() && *options.common.frames == 9, "common frames retained");
    expect(options.scenePath.string() == "arena.scene.json", "scene path override");
    expect(options.controlProfilePath.string() == "profile.controls.json", "control profile override");
    expect(options.networkServer == "127.0.0.1:41000", "network server override");
    expect(options.networkClientId == 37, "network client id override");
    expect(options.orbitCamera, "orbit camera flag");
    expect(!options.visualLab, "standard mode remains non visual-lab");
}

void parseVisualLabOptions()
{
    const vac::viewer::ViewerOptions options =
        parse({"viewer",
               "--visual-lab",
               "--frames", "2",
               "--move", "moves/swing.move.json",
               "--proxy-animation", "animations/swing.proxy_animation.json",
               "--scenario", "scenarios/swing.scenario.json",
               "--result-file", "viewer_result.json",
               "--offline",
               "--hidden-window"});

    expect(options.visualLab, "visual lab flag");
    expect(options.frames == 2, "visual lab frames");
    expect(options.visualLabMovePath.string() == "moves/swing.move.json", "visual lab move path");
    expect(options.visualLabProxyAnimationPath.string() == "animations/swing.proxy_animation.json",
           "visual lab proxy animation path");
    expect(options.visualLabScenarioPath.string() == "scenarios/swing.scenario.json",
           "visual lab scenario path");
    expect(options.common.resultFile.has_value() && options.common.resultFile->string() == "viewer_result.json",
           "visual lab result file");
    expect(options.common.offline, "offline common option remains supported");
    expect(options.common.hiddenWindow, "hidden-window common option remains supported");
}

void rejectInvalidOptionCombinations()
{
    expectParseFailure({"viewer", "--move", "moves/swing.move.json"}, "--move requires --visual-lab");
    expectParseFailure({"viewer", "--visual-lab", "--scenario", "scenario.json", "--scene", "scene.json"},
                       "--scenario cannot be combined with --scene");
    expectParseFailure({"viewer", "--net-client-id", "256"}, "--net-client-id must be between 0 and 255");
    expectParseFailure({"viewer", "--headless"}, "--headless is not supported");
}

void writeResultFiles()
{
    const std::filesystem::path resultPath =
        std::filesystem::temp_directory_path() / "vulkan_action_client_viewer_module_result.json";
    std::error_code error;
    std::filesystem::remove(resultPath, error);

    vac::viewer::ViewerOptions options;
    options.frames = 4;
    options.common.frames = 4;
    options.common.resultFile = resultPath;

    vac::viewer::writeSuccessResultFile(options, {"camera=ok"});
    nlohmann::json result = readJson(resultPath);
    expect(result.at("host").get<std::string>() == "vulkan_scene_viewer", "success host");
    expect(result.at("status").get<std::string>() == "ok", "success status");
    expect(result.at("message").get<std::string>() == "Rendered 4 frame(s)", "success message");
    expect(result.at("frames").get<int>() == 4, "success frames");
    expect(result.at("diagnostics").at(0).get<std::string>() == "camera=ok", "success diagnostics");

    options.visualLab = true;
    options.frames = 2;
    options.common.frames = 2;
    vac::viewer::writeSuccessResultFile(options, {});
    result = readJson(resultPath);
    expect(result.at("message").get<std::string>() == "Visual lab rendered 2 frame(s)",
           "visual lab success message");

    vac::viewer::writeErrorResultFile(resultPath, "boom");
    result = readJson(resultPath);
    expect(result.at("host").get<std::string>() == "vulkan_scene_viewer", "error host");
    expect(result.at("status").get<std::string>() == "error", "error status");
    expect(result.at("message").get<std::string>() == "boom", "error message");

    std::filesystem::remove(resultPath, error);
}

void cameraBuildsStableView()
{
    const vac::viewer::CameraView camera = vac::viewer::buildCameraView({
        .worldBoundsValid = true,
        .worldBoundsMin = {-10.0f, 0.0f, -4.0f},
        .worldBoundsMax = {10.0f, 2.0f, 4.0f},
        .anchorTranslation = {1.0f, 2.0f, 3.0f},
        .yawDegrees = 180.0f,
        .pitchDegrees = 24.0f,
        .distanceWorldUnits = vac::viewer::kDefaultCameraDistanceWorldUnits,
    });

    expect(std::abs(camera.worldRadius - 10.0f) < 0.001f, "world radius derives from bounds");
    expect(std::isfinite(camera.eye.x) && std::isfinite(camera.eye.y) && std::isfinite(camera.eye.z),
           "camera eye finite");
    expect(std::isfinite(camera.target.x) && std::isfinite(camera.target.y) && std::isfinite(camera.target.z),
           "camera target finite");
}
} // namespace

int main()
{
    try {
        parseStandardViewerOptions();
        parseVisualLabOptions();
        rejectInvalidOptionCombinations();
        writeResultFiles();
        cameraBuildsStableView();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "viewer_module_tests failed: " << error.what() << '\n';
        return 1;
    }
}
