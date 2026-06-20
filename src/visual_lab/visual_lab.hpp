#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "combat/combat_scenario.hpp"
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

struct VisualLabPlaybackState
{
    uint32_t durationTicks = 0;
    uint32_t currentTick = 0;
    bool paused = true;
    bool running = false;
};

struct VisualLabScenarioEvidenceSummary
{
    uint32_t eventCount = 0;
    uint32_t firstEventTick = 0;
    uint32_t lastEventTick = 0;
    uint32_t effectEventCount = 0;
    uint32_t hitEventCount = 0;
    uint32_t blockedHitEventCount = 0;
    uint32_t totalDamage = 0;
    uint32_t maxDamage = 0;
    uint32_t lowestRemainingHealth = 0;
    std::vector<std::string> reactionMoves;
};

struct VisualLabScene
{
    SceneRuntime scene;
    SceneDrawData debugDraw;
    VisualLabSummary summary;
    VisualLabPlaybackState playback;
    VisualLabScenarioEvidenceSummary scenarioEvidence;
    std::vector<std::string> diagnostics;
    std::vector<std::string> resultDiagnostics;
};

VisualLabAssetPaths defaultVisualLabAssetPaths();
VisualLabScene buildVisualLabScene(const VisualLabAssetPaths &paths);
VisualLabScene buildVisualLabSceneFromScenario(const std::filesystem::path &scenarioPath);
VisualLabPlaybackState makePlaybackState(uint32_t durationTicks);
VisualLabPlaybackState setPlaybackPaused(VisualLabPlaybackState state, bool paused);
VisualLabPlaybackState resetPlayback(VisualLabPlaybackState state);
VisualLabPlaybackState stepPlayback(VisualLabPlaybackState state, uint32_t ticks = 1);
VisualLabPlaybackState seekPlayback(VisualLabPlaybackState state, uint32_t tick);
VisualLabScenarioEvidenceSummary summarizeScenarioEvidence(const combat::ScenarioTrace &trace);
std::vector<std::string> summaryDiagnostics(const VisualLabSummary &summary);
std::vector<std::string> playbackDiagnostics(const VisualLabPlaybackState &state);
std::vector<std::string> scenarioEvidenceDiagnostics(const VisualLabScenarioEvidenceSummary &summary);
} // namespace vac::visual_lab
