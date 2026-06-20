#include "scene/scene_runtime.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

#include <fmt/format.h>

namespace
{
std::filesystem::path scenePathFromArgs(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view{argv[i]} == "--scene") {
            return argv[i + 1];
        }
    }

    return vac::defaultProjectRoot() / "config/scenes/bootstrap.scene.json";
}
} // namespace

int main(int argc, char **argv)
{
    try {
        const std::filesystem::path projectRoot = vac::defaultProjectRoot();
        const std::filesystem::path scenePath = scenePathFromArgs(argc, argv);
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
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "scene_probe failed: " << error.what() << "\n";
        return 1;
    }
}
