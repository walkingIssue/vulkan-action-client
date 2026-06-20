#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "content/authoring_scene.hpp"
#include "render/scene_geometry.hpp"
#include "scene/scene_runtime.hpp"
#include "simulation/runtime_world.hpp"

namespace vac::visual_lab
{
struct VisualLabAssetPaths
{
    std::filesystem::path mapPath;
    std::filesystem::path movePath;
    std::filesystem::path proxyAnimationPath;
};

struct VisualLabSummary
{
    uint32_t primitiveCount = 0;
    uint32_t colliderCount = 0;
    uint32_t spawnPointCount = 0;
    uint32_t actorRootCount = 0;
    uint32_t movePhaseWindowCount = 0;
    uint32_t hitboxVolumeCount = 0;
    uint32_t hurtboxVolumeCount = 0;
    uint32_t proxySocketMarkerCount = 0;
    uint32_t rootMotionPathSegmentCount = 0;
    uint32_t interpolationSampleCount = 0;
    uint32_t debugLineVertexCount = 0;
};

struct VisualLabScene
{
    SceneRuntime scene;
    SceneDrawData debugDraw;
    VisualLabSummary summary;
    std::vector<std::string> diagnostics;
};

VisualLabAssetPaths defaultVisualLabAssetPaths();
VisualLabScene buildVisualLabScene(const VisualLabAssetPaths &paths);
std::vector<std::string> summaryDiagnostics(const VisualLabSummary &summary);
} // namespace vac::visual_lab
