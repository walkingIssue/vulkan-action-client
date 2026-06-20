#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "combat/combat_collision.hpp"
#include "simulation/runtime_world.hpp"

namespace vac::content
{
struct RuntimeWorld;
}

namespace vac::combat
{
inline constexpr uint32_t kCombatScenarioSchemaVersion = 1;

struct ScenarioDiagnostic
{
    std::string severity = "error";
    std::string code;
    std::string fieldPath;
    std::string message;
};

struct ScenarioHurtbox
{
    CombatVolumeKind kind = CombatVolumeKind::box;
    glm::vec3 size{1.0f, 2.0f, 1.0f};
    glm::vec3 offset{0.0f, 1.0f, 0.0f};
};

struct ScenarioActor
{
    simulation::RuntimeActorId id;
    std::string spawnId;
    std::string team;
    std::optional<glm::vec3> translation;
    std::optional<float> yawDegrees;
    ScenarioHurtbox hurtbox;
};

struct ScenarioInputCommand
{
    uint32_t tick = 0;
    simulation::RuntimeActorId actorId;
    std::string command;
};

struct ScenarioAnimationBinding
{
    std::string moveLogicalId;
    std::filesystem::path path;
};

struct CombatScenario
{
    std::filesystem::path sourcePath;
    uint32_t schemaVersion = kCombatScenarioSchemaVersion;
    std::string logicalId;
    uint32_t durationTicks = 0;
    uint64_t seed = 0;
    std::filesystem::path mapPath;
    std::vector<std::filesystem::path> movePaths;
    std::vector<ScenarioAnimationBinding> animations;
    std::vector<ScenarioActor> actors;
    std::vector<ScenarioInputCommand> inputs;
    std::filesystem::path goldenPath;
};

struct ScenarioResolvedAnimationPath
{
    std::string moveLogicalId;
    std::filesystem::path path;
};

struct CombatScenarioResolvedPaths
{
    std::filesystem::path mapPath;
    std::vector<std::filesystem::path> movePaths;
    std::vector<ScenarioResolvedAnimationPath> animations;
    std::filesystem::path goldenPath;
};

struct ScenarioTraceEvent
{
    uint32_t sequence = 0;
    uint32_t tick = 0;
    std::string kind;
    uint32_t actorId = 0;
    uint32_t targetId = 0;
    std::string move;
    uint32_t moveTick = 0;
    std::string label;
};

struct ScenarioTrace
{
    std::string scenarioId;
    uint64_t seed = 0;
    uint32_t ticksRun = 0;
    std::string finalStateHash;
    std::vector<ScenarioTraceEvent> events;
};

struct ScenarioRunResult
{
    std::string status = "ok";
    std::string message;
    CombatScenario scenario;
    ScenarioTrace trace;
    std::vector<ScenarioDiagnostic> diagnostics;
    bool goldenCompared = false;
    bool goldenMatched = false;
    std::filesystem::path goldenPath;
};

struct ScenarioRunOptions
{
    std::optional<uint32_t> overrideTicks;
    std::optional<uint64_t> overrideSeed;
    bool updateGolden = false;
    bool allowGoldenUpdate = false;
};

CombatScenario combatScenarioFromJson(const nlohmann::json &document, std::filesystem::path sourcePath = {});
CombatScenario loadCombatScenario(const std::filesystem::path &path);
std::filesystem::path resolveScenarioPath(const CombatScenario &scenario, const std::filesystem::path &path);
CombatScenarioResolvedPaths resolveCombatScenarioPaths(const CombatScenario &scenario);
simulation::RuntimeWorld makeScenarioRuntimeWorld(const CombatScenario &scenario,
                                                  const content::RuntimeWorld &contentWorld);
nlohmann::json toJson(const ScenarioTraceEvent &event);
nlohmann::json toJson(const ScenarioTrace &trace);
nlohmann::json toJson(const ScenarioRunResult &result);
ScenarioRunResult runCombatScenario(CombatScenario scenario, const ScenarioRunOptions &options = {});
void writeScenarioResultFile(const std::filesystem::path &path, const ScenarioRunResult &result);
void writeGoldenTraceFile(const std::filesystem::path &path, const ScenarioTrace &trace);
ScenarioTrace loadGoldenTraceFile(const std::filesystem::path &path);
} // namespace vac::combat
