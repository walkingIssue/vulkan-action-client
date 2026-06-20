#include "visual_lab/visual_lab.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

bool containsDiagnostic(const std::vector<std::string> &diagnostics, std::string_view expected)
{
    return std::find(diagnostics.begin(), diagnostics.end(), expected) != diagnostics.end();
}

void defaultLabBuildsSprintDebugScene()
{
    const vac::visual_lab::VisualLabScene scene =
        vac::visual_lab::buildVisualLabScene(vac::visual_lab::defaultVisualLabAssetPaths());

    expect(scene.diagnostics.empty(), "default visual lab has no diagnostics");
    expect(scene.scene.name == "sprint01_visual_lab", "visual lab scene name");
    expect(scene.scene.models.empty(), "visual lab uses primitives, not imported models");
    expect(scene.summary.primitiveCount == 3, "visual lab primitive count");
    expect(scene.summary.colliderCount == 3, "visual lab collider count");
    expect(scene.summary.spawnPointCount == 2, "visual lab spawn count");
    expect(scene.summary.actorRootCount == 2, "visual lab actor root count");
    expect(scene.summary.movePhaseWindowCount == 3, "visual lab move phase window count");
    expect(scene.summary.hitboxVolumeCount == 1, "visual lab hitbox count");
    expect(scene.summary.hurtboxVolumeCount == 1, "visual lab hurtbox count");
    expect(scene.summary.proxySocketMarkerCount == 3, "visual lab proxy socket marker count");
    expect(scene.summary.rootMotionPathSegmentCount == 2, "visual lab root-motion path segment count");
    expect(scene.summary.interpolationSampleCount == 3, "visual lab interpolation samples");
    expect(scene.summary.debugLineVertexCount == scene.debugDraw.lineVertices.size(), "line count mirrors draw data");
    expect(scene.summary.debugLineVertexCount > 80, "visual lab has substantial debug lines");
    expect(scene.scene.procedural.size() == scene.summary.primitiveCount + scene.summary.actorRootCount,
           "visual lab procedural primitives include map and actor roots");
}

void missingMapReportsDiagnostic()
{
    vac::visual_lab::VisualLabAssetPaths paths = vac::visual_lab::defaultVisualLabAssetPaths();
    paths.mapPath = paths.mapPath.parent_path() / "missing_visual_lab_map.map.json";

    try {
        (void)vac::visual_lab::buildVisualLabScene(paths);
        expect(false, "missing map throws");
    } catch (const std::exception &error) {
        expect(std::string_view{error.what()}.find("Could not open") != std::string_view::npos,
               "missing map reports open failure");
    }
}

void summaryDiagnosticsAreStructured()
{
    vac::visual_lab::VisualLabSummary summary;
    summary.primitiveCount = 3;
    summary.spawnPointCount = 2;
    summary.debugLineVertexCount = 128;

    const std::vector<std::string> diagnostics = vac::visual_lab::summaryDiagnostics(summary);
    expect(!diagnostics.empty() && diagnostics.front() == "visualLab=true", "summary marks visual lab mode");
    expect(std::find(diagnostics.begin(), diagnostics.end(), "primitiveCount=3") != diagnostics.end(),
           "summary includes primitive count");
    expect(std::find(diagnostics.begin(), diagnostics.end(), "debugLineVertexCount=128") != diagnostics.end(),
           "summary includes debug line count");
}

void playbackModelClampsStepsAndResets()
{
    vac::visual_lab::VisualLabPlaybackState state = vac::visual_lab::makePlaybackState(36);
    expect(state.currentTick == 0, "playback starts at tick zero");
    expect(state.durationTicks == 36, "playback stores duration");
    expect(state.paused && !state.running, "playback starts paused");

    state = vac::visual_lab::setPlaybackPaused(state, false);
    expect(!state.paused && state.running, "playback can run");

    state = vac::visual_lab::stepPlayback(state, 5);
    expect(state.currentTick == 5, "playback steps by requested ticks");
    expect(state.paused && !state.running, "manual step pauses playback");

    state = vac::visual_lab::seekPlayback(state, 500);
    expect(state.currentTick == 36, "playback seek clamps to duration");

    state = vac::visual_lab::stepPlayback(state);
    expect(state.currentTick == 36, "playback step clamps at duration");

    state = vac::visual_lab::resetPlayback(state);
    expect(state.currentTick == 0, "playback reset returns to tick zero");
    expect(state.paused && !state.running, "playback reset pauses");

    const std::vector<std::string> diagnostics = vac::visual_lab::playbackDiagnostics(state);
    expect(containsDiagnostic(diagnostics, "visualLabPlayback=true"), "playback diagnostics enabled");
    expect(containsDiagnostic(diagnostics, "playbackCanStepForward=true"), "playback diagnostics expose step bound");
}

void scenarioEvidenceSummarizesCombatTraceFields()
{
    const std::filesystem::path scenarioPath =
        vac::defaultProjectRoot() / "tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json";
    const vac::combat::ScenarioRunResult result =
        vac::combat::runCombatScenario(vac::combat::loadCombatScenario(scenarioPath));
    const vac::visual_lab::VisualLabScenarioEvidenceSummary summary =
        vac::visual_lab::summarizeScenarioEvidence(result.trace);

    expect(summary.eventCount == 13, "scenario evidence event count");
    expect(summary.firstEventTick == 0, "scenario evidence first tick");
    expect(summary.lastEventTick == 33, "scenario evidence last tick");
    expect(summary.effectEventCount == 1, "scenario evidence effect event count");
    expect(summary.hitEventCount == 1, "scenario evidence hit event count");
    expect(summary.blockedHitEventCount == 0, "scenario evidence blocked hit count");
    expect(summary.totalDamage == 12, "scenario evidence total damage");
    expect(summary.maxDamage == 12, "scenario evidence max damage");
    expect(summary.lowestRemainingHealth == 88, "scenario evidence remaining health");
    expect(summary.reactionMoves.size() == 1 && summary.reactionMoves.front() == "move.hit_reaction",
           "scenario evidence reaction move");

    const std::vector<std::string> diagnostics = vac::visual_lab::scenarioEvidenceDiagnostics(summary);
    expect(containsDiagnostic(diagnostics, "scenarioEvidenceEventCount=13"), "evidence diagnostics event count");
    expect(containsDiagnostic(diagnostics, "scenarioEvidenceTotalDamage=12"), "evidence diagnostics damage");
    expect(containsDiagnostic(diagnostics, "scenarioEvidenceHasReaction=true"), "evidence diagnostics reaction flag");
    expect(containsDiagnostic(diagnostics, "scenarioEvidenceReactionMoves=move.hit_reaction"),
           "evidence diagnostics reaction list");
}

void scenarioLabBuildsScenarioDiagnostics()
{
    const std::filesystem::path scenarioPath =
        vac::defaultProjectRoot() / "tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json";
    const vac::visual_lab::VisualLabScene scene = vac::visual_lab::buildVisualLabSceneFromScenario(scenarioPath);

    expect(scene.diagnostics.empty(), "scenario visual lab has no diagnostics");
    expect(scene.summary.primitiveCount == 3, "scenario visual lab primitive count");
    expect(scene.summary.actorRootCount == 2, "scenario visual lab actor roots from scenario actors");
    expect(scene.summary.hitboxVolumeCount == 1, "scenario visual lab hitbox count");
    expect(scene.summary.debugLineVertexCount > 80, "scenario visual lab has substantial debug lines");

    expect(std::find(scene.resultDiagnostics.begin(), scene.resultDiagnostics.end(), "visualLabSource=scenario") !=
               scene.resultDiagnostics.end(),
           "scenario diagnostics mark visual lab source");
    expect(std::find(scene.resultDiagnostics.begin(),
                     scene.resultDiagnostics.end(),
                     "scenarioId=scenario.sword_light_hits_idle_target") != scene.resultDiagnostics.end(),
           "scenario diagnostics include scenario id");
    expect(std::find(scene.resultDiagnostics.begin(), scene.resultDiagnostics.end(), "scenarioStatus=ok") !=
               scene.resultDiagnostics.end(),
           "scenario diagnostics include scenario status");
    expect(std::find(scene.resultDiagnostics.begin(), scene.resultDiagnostics.end(), "scenarioGoldenCompared=true") !=
               scene.resultDiagnostics.end(),
           "scenario diagnostics include golden compared");
    expect(std::find(scene.resultDiagnostics.begin(), scene.resultDiagnostics.end(), "scenarioGoldenMatched=true") !=
               scene.resultDiagnostics.end(),
           "scenario diagnostics include golden matched");
    expect(std::find(scene.resultDiagnostics.begin(), scene.resultDiagnostics.end(), "scenarioTraceEventCount=13") !=
               scene.resultDiagnostics.end(),
           "scenario diagnostics include trace event count");
    expect(std::find(scene.resultDiagnostics.begin(),
                     scene.resultDiagnostics.end(),
                     "scenarioFinalStateHash=0xf73237aa3baea830") != scene.resultDiagnostics.end(),
           "scenario diagnostics include final state hash");

    expect(scene.playback.durationTicks == 36, "scenario visual lab playback duration");
    expect(scene.playback.currentTick == 0, "scenario visual lab playback starts at zero");
    expect(scene.playback.paused && !scene.playback.running, "scenario visual lab playback starts paused");
    expect(scene.scenarioEvidence.eventCount == 13, "scenario visual lab evidence count");
    expect(scene.scenarioEvidence.totalDamage == 12, "scenario visual lab evidence damage");
    expect(containsDiagnostic(scene.resultDiagnostics, "visualLabPlayback=true"),
           "scenario diagnostics include playback marker");
    expect(containsDiagnostic(scene.resultDiagnostics, "playbackPreviewStepTick=1"),
           "scenario diagnostics include preview step");
    expect(containsDiagnostic(scene.resultDiagnostics, "playbackPreviewResetTick=0"),
           "scenario diagnostics include preview reset");
    expect(containsDiagnostic(scene.resultDiagnostics, "playbackPreviewSeekEndTick=36"),
           "scenario diagnostics include bounded seek");
    expect(containsDiagnostic(scene.resultDiagnostics, "scenarioEvidenceEventCount=13"),
           "scenario diagnostics include evidence event count");
    expect(containsDiagnostic(scene.resultDiagnostics, "scenarioEvidenceTotalDamage=12"),
           "scenario diagnostics include damage summary");
    expect(containsDiagnostic(scene.resultDiagnostics, "scenarioEvidenceReactionMoves=move.hit_reaction"),
           "scenario diagnostics include reaction summary");
}
} // namespace

int main()
{
    try {
        defaultLabBuildsSprintDebugScene();
        missingMapReportsDiagnostic();
        summaryDiagnosticsAreStructured();
        playbackModelClampsStepsAndResets();
        scenarioEvidenceSummarizesCombatTraceFields();
        scenarioLabBuildsScenarioDiagnostics();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
