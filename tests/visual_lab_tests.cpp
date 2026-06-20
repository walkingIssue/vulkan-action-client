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
} // namespace

int main()
{
    try {
        defaultLabBuildsSprintDebugScene();
        missingMapReportsDiagnostic();
        summaryDiagnosticsAreStructured();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
