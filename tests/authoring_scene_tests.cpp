#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/authoring_scene.hpp"

namespace
{
int g_failures = 0;

std::filesystem::path projectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path validMapPath() { return projectRoot() / "content/maps/basic_primitive_arena.map.json"; }

std::filesystem::path invalidFixturePath(std::string_view fileName)
{
    return projectRoot() / "tests/fixtures/authoring" / fileName;
}

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::cerr << "FAIL: " << message << " expected " << expected << " got " << actual << '\n';
    }
}

void expectVec3(glm::vec3 actual, glm::vec3 expected, float tolerance, std::string_view message)
{
    expectNear(actual.x, expected.x, tolerance, std::string{message} + " x");
    expectNear(actual.y, expected.y, tolerance, std::string{message} + " y");
    expectNear(actual.z, expected.z, tolerance, std::string{message} + " z");
}

const vac::content::ContentDiagnostic *findDiagnostic(const vac::content::ValidationResult &result,
                                                      std::string_view code)
{
    for (const vac::content::ContentDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return &diagnostic;
        }
    }
    return nullptr;
}

void expectDiagnostic(const vac::content::ValidationResult &result, std::string_view code, std::string_view objectId,
                      std::string_view component, std::string_view fieldPath)
{
    const vac::content::ContentDiagnostic *diagnostic = findDiagnostic(result, code);
    expect(diagnostic != nullptr, std::string{"diagnostic exists: "} + std::string{code});
    if (diagnostic == nullptr) {
        return;
    }

    expect(diagnostic->severity == "error", std::string{"diagnostic severity for "} + std::string{code});
    expect(diagnostic->objectId == objectId, std::string{"diagnostic object for "} + std::string{code});
    expect(diagnostic->component == component, std::string{"diagnostic component for "} + std::string{code});
    expect(diagnostic->fieldPath == fieldPath, std::string{"diagnostic field for "} + std::string{code});
    expect(!diagnostic->file.empty(), std::string{"diagnostic file for "} + std::string{code});
    expect(!diagnostic->message.empty(), std::string{"diagnostic message for "} + std::string{code});
}

void validMapRoundTripsCanonically()
{
    const vac::content::AuthoringScene scene = vac::content::loadAuthoringScene(validMapPath());
    const vac::content::ValidationResult validation = vac::content::validateAuthoringScene(scene);
    expect(validation.ok(), "valid primitive arena validates");

    const std::string canonical = vac::content::toCanonicalJsonString(scene);
    const vac::content::AuthoringScene roundTrip =
        vac::content::authoringSceneFromJson(nlohmann::json::parse(canonical), validMapPath());
    expect(vac::content::toCanonicalJsonString(roundTrip) == canonical,
           "valid primitive arena round-trips canonically");

    const std::filesystem::path savedPath =
        std::filesystem::current_path() / "test-artifacts/authoring_scene_roundtrip.map.json";
    std::filesystem::create_directories(savedPath.parent_path());
    vac::content::saveAuthoringScene(scene, savedPath);
    const vac::content::AuthoringScene savedRoundTrip = vac::content::loadAuthoringScene(savedPath);
    expect(vac::content::toCanonicalJsonString(savedRoundTrip) == canonical,
           "saved primitive arena file round-trips canonically");

    vac::content::AuthoringScene shuffled = scene;
    std::reverse(shuffled.objects.begin(), shuffled.objects.end());
    std::reverse(shuffled.spawnPoints.begin(), shuffled.spawnPoints.end());
    expect(vac::content::toCanonicalJsonString(shuffled) == canonical,
           "canonical output is stable across object and spawn ordering");
}

void validMapCompilesToRuntimeWorld()
{
    const vac::content::AuthoringScene scene = vac::content::loadAuthoringScene(validMapPath());
    const vac::content::CompileResult compiled = vac::content::compileRuntimeWorld(scene);

    expect(compiled.ok(), "valid primitive arena compiles");
    expect(compiled.world.entities.size() == 3, "compiled runtime entity count");
    expect(compiled.world.colliders.size() == 3, "compiled runtime collider count");
    expect(compiled.world.spawnPoints.size() == 2, "compiled runtime spawn count");

    expectVec3(compiled.world.worldBounds.min, {-25.0f, -1.0f, -25.0f}, 0.001f, "compiled world bounds min");
    expectVec3(compiled.world.worldBounds.max, {25.0f, 10.0f, 25.0f}, 0.001f, "compiled world bounds max");
    expectNear(compiled.world.worldBounds.killHeight, -8.0f, 0.001f, "compiled world kill height");

    expect(!compiled.world.entities.empty() && compiled.world.entities.at(0).id.value == 1,
           "first runtime entity id is stable");
    expect(!compiled.world.entities.empty() && compiled.world.entities.at(0).authoredId.value == "arena_floor",
           "runtime entities are sorted by authored id");
    expectVec3(compiled.world.entities.at(0).localBounds.min, {-20.0f, -0.125f, -20.0f}, 0.001f,
               "arena floor runtime bounds min");
    expectVec3(compiled.world.entities.at(0).localBounds.max, {20.0f, 0.125f, 20.0f}, 0.001f,
               "arena floor runtime bounds max");

    expect(compiled.world.colliders.at(0).entityId.value == 1, "first collider references first runtime entity");
    expect(compiled.world.colliders.at(0).layer == "world", "collider metadata keeps layer");
    expect(compiled.world.spawnPoints.at(0).id == "enemy_spawn", "spawn points are sorted by id");
    expect(compiled.world.spawnPoints.at(1).id == "player_spawn", "second sorted spawn point");
}

void invalidFixtureReports(std::string_view fileName, std::string_view code, std::string_view objectId,
                           std::string_view component, std::string_view fieldPath)
{
    const vac::content::AuthoringScene scene = vac::content::loadAuthoringScene(invalidFixturePath(fileName));
    const vac::content::ValidationResult validation = vac::content::validateAuthoringScene(scene);
    expect(!validation.ok(), std::string{"invalid fixture fails validation: "} + std::string{fileName});
    expectDiagnostic(validation, code, objectId, component, fieldPath);

    const vac::content::CompileResult compiled = vac::content::compileRuntimeWorld(scene);
    expect(!compiled.ok(), std::string{"invalid fixture does not compile: "} + std::string{fileName});
}

void invalidMapsProduceStructuredDiagnostics()
{
    invalidFixtureReports("duplicate_ids.map.json", "duplicate_object_id", "blocker", "Object", "objects/blocker");
    invalidFixtureReports("invalid_collider.map.json", "invalid_collider", "blocker", "Collider",
                          "objects/blocker/collider/size");
    invalidFixtureReports("invalid_transform.map.json", "invalid_transform", "blocker", "Transform",
                          "objects/blocker/transform");
    invalidFixtureReports("invalid_world_bounds.map.json", "invalid_world_bounds", "", "WorldBounds", "worldBounds");
    invalidFixtureReports("missing_parent.map.json", "missing_parent", "child", "Transform", "objects/child/parent");
    invalidFixtureReports("missing_spawn_points.map.json", "missing_spawn_points", "", "SpawnPoint", "spawnPoints");
}
} // namespace

int main()
{
    try {
        validMapRoundTripsCanonically();
        validMapCompilesToRuntimeWorld();
        invalidMapsProduceStructuredDiagnostics();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
