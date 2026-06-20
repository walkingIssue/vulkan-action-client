#include "scene/scene_runtime.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>

#include <fmt/format.h>

#include "host/host_cli.hpp"

namespace
{
std::filesystem::path scenePathFromOptions(const vac::host::CommonHostOptions &options)
{
    if (options.scene.has_value()) {
        return *options.scene;
    }
    return vac::defaultProjectRoot() / "config/scenes/bootstrap.scene.json";
}
} // namespace

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> resultFile;
    try {
        vac::host::CommandLine commandLine{argc, argv};
        const vac::host::CommonHostOptions commonOptions = vac::host::parseCommonOptions(commandLine);
        resultFile = commonOptions.resultFile;
        vac::host::rejectUnsupportedCommonOptions(commonOptions, {"--scene", "--result-file"});
        commandLine.rejectUnknown();

        const std::filesystem::path projectRoot = vac::defaultProjectRoot();
        const std::filesystem::path scenePath = scenePathFromOptions(commonOptions);
        const vac::SceneRuntime scene = vac::loadScene(scenePath, projectRoot);

        std::cout << fmt::format("Scene '{}'\n", scene.name);
        std::cout << fmt::format("  models: {}\n", scene.models.size());
        for (const auto &[id, model] : scene.models) {
            std::cout << fmt::format(
                "    {}: meshes={} materials={} animations={} vertices={} triangles={} bounds={}\n",
                id,
                model.meshCount,
                model.materialCount,
                model.animationCount,
                model.vertexCount,
                model.triangleCount,
                vac::formatBounds(model.localBounds));
        }

        std::cout << fmt::format("  procedural: {}\n", scene.procedural.size());
        for (const vac::ProceduralInstance &instance : scene.procedural) {
            std::cout << fmt::format(
                "    {} [{}] type={} bounds={}\n",
                instance.id,
                instance.name,
                instance.type,
                vac::formatBounds(instance.worldBounds));
        }

        std::cout << fmt::format("  instances: {}\n", scene.instances.size());
        for (const vac::SceneInstance &instance : scene.instances) {
            std::cout << fmt::format(
                "    {} [{}] model={} pos=({:.2f}, {:.2f}, {:.2f}) rot=({:.1f}, {:.1f}, {:.1f}) bounds={}\n",
                instance.id,
                instance.name,
                instance.modelId,
                instance.transform.translation.x,
                instance.transform.translation.y,
                instance.transform.translation.z,
                instance.transform.rotationDegrees.x,
                instance.transform.rotationDegrees.y,
                instance.transform.rotationDegrees.z,
                vac::formatBounds(instance.worldBounds));
        }

        std::cout << fmt::format("  world bounds: {}\n", vac::formatBounds(scene.worldBounds));
        if (resultFile.has_value()) {
            vac::host::HostResult result = vac::host::resultFromOptions("scene_probe", commonOptions);
            result.message = fmt::format("Loaded scene '{}'", scene.name);
            vac::host::writeResultFile(*resultFile, result);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "scene_probe failed: " << error.what() << "\n";
        if (resultFile.has_value()) {
            try {
                vac::host::HostResult result;
                result.host = "scene_probe";
                result.status = "error";
                result.message = error.what();
                vac::host::writeResultFile(*resultFile, result);
            } catch (const std::exception &resultError) {
                std::cerr << "scene_probe could not write result file: " << resultError.what() << "\n";
            }
        }
        return 1;
    }
}
