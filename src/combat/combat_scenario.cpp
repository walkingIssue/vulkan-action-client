#include "combat/combat_scenario.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <glm/trigonometric.hpp>

#include "animation/proxy_animation.hpp"
#include "combat/combat_runtime.hpp"
#include "content/authoring_scene.hpp"
#include "content/move_asset.hpp"

namespace vac::combat
{
namespace
{
using json = nlohmann::json;

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr uint32_t kScenarioInvulnerableTag = 1;

struct ScenarioCharacterDefinition
{
    std::filesystem::path sourcePath;
    std::string logicalId;
    std::string label;
    ScenarioCombatBridge combatBridge;
    ScenarioHurtbox hurtbox;
    std::vector<std::string> defaultMoves;
};

struct ScenarioCharacterLoadResult
{
    std::vector<ScenarioCharacterDefinition> characters;
    std::vector<ScenarioDiagnostic> diagnostics;
};

std::filesystem::path projectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

glm::vec3 readVec3(const json &value, glm::vec3 fallback)
{
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

json vec3ToJson(glm::vec3 value) { return json::array({value.x, value.y, value.z}); }

ScenarioCombatBridge readCombatBridge(const json &value)
{
    ScenarioCombatBridge bridge;
    if (!value.is_object()) {
        return bridge;
    }
    bridge.maxHealth = value.value("maxHealth", bridge.maxHealth);
    bridge.health = value.value("health", bridge.maxHealth);
    return bridge;
}

std::optional<uint32_t> readUnsignedField(const json &value, std::initializer_list<std::string_view> names)
{
    if (!value.is_object()) {
        return std::nullopt;
    }

    for (std::string_view name : names) {
        const auto it = value.find(std::string{name});
        if (it == value.end()) {
            continue;
        }
        if (it->is_number_unsigned()) {
            const uint64_t raw = it->get<uint64_t>();
            if (raw <= std::numeric_limits<uint32_t>::max()) {
                return static_cast<uint32_t>(raw);
            }
            return std::nullopt;
        }
        if (it->is_number_integer()) {
            const int64_t raw = it->get<int64_t>();
            if (raw >= 0 && raw <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                return static_cast<uint32_t>(raw);
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<CombatVolumeKind> tryVolumeKindFromString(const std::string &value)
{
    if (value == "box") {
        return CombatVolumeKind::box;
    }
    if (value == "sphere") {
        return CombatVolumeKind::sphere;
    }
    if (value == "capsule") {
        return CombatVolumeKind::capsule;
    }
    return std::nullopt;
}

CombatVolumeKind volumeKindFromString(const std::string &value)
{
    return tryVolumeKindFromString(value).value_or(CombatVolumeKind::box);
}

std::string toString(CombatVolumeKind kind)
{
    switch (kind) {
    case CombatVolumeKind::sphere:
        return "sphere";
    case CombatVolumeKind::capsule:
        return "capsule";
    case CombatVolumeKind::box:
        return "box";
    }
    return "box";
}

ScenarioHurtbox readHurtbox(const json &value, ScenarioHurtbox fallback)
{
    if (!value.is_object()) {
        return fallback;
    }

    fallback.kind = volumeKindFromString(value.value("shape", "box"));
    fallback.size = readVec3(value.value("size", json::array()), fallback.size);
    fallback.offset = readVec3(value.value("offset", json::array()), fallback.offset);
    return fallback;
}

bool isFiniteVec3(glm::vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::optional<glm::vec3> readRequiredVec3(const json &value, bool requirePositive)
{
    if (!value.is_array() || value.size() != 3) {
        return std::nullopt;
    }

    glm::vec3 result{};
    for (size_t i = 0; i < 3; ++i) {
        if (!value.at(i).is_number()) {
            return std::nullopt;
        }
        result[static_cast<glm::length_t>(i)] = value.at(i).get<float>();
    }

    if (!isFiniteVec3(result)) {
        return std::nullopt;
    }
    if (requirePositive && (result.x <= 0.0f || result.y <= 0.0f || result.z <= 0.0f)) {
        return std::nullopt;
    }
    return result;
}

bool isValidHurtbox(const ScenarioHurtbox &hurtbox)
{
    return isFiniteVec3(hurtbox.size) && isFiniteVec3(hurtbox.offset) && hurtbox.size.x > 0.0f &&
           hurtbox.size.y > 0.0f && hurtbox.size.z > 0.0f;
}

glm::vec3 rotateYaw(glm::vec3 value, float yawDegrees)
{
    const float yaw = glm::radians(yawDegrees);
    const float s = std::sin(yaw);
    const float c = std::cos(yaw);
    return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

std::filesystem::path resolvePath(const CombatScenario &scenario, const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute()) {
        return path;
    }

    if (!scenario.sourcePath.empty()) {
        const std::filesystem::path relativeToScenario = scenario.sourcePath.parent_path() / path;
        if (std::filesystem::exists(relativeToScenario)) {
            return relativeToScenario;
        }
    }

    return projectRoot() / path;
}

void addDiagnostic(std::vector<ScenarioDiagnostic> &diagnostics, std::string code, std::string fieldPath,
                   std::string message)
{
    diagnostics.push_back({"error", std::move(code), std::move(fieldPath), std::move(message)});
}

std::string pathMessage(const std::filesystem::path &path)
{
    return path.empty() ? "<empty>" : path.generic_string();
}

bool fileExists(const std::filesystem::path &path)
{
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

void addMissingFileDiagnostic(std::vector<ScenarioDiagnostic> &diagnostics, std::string code, std::string fieldPath,
                              std::string label, const std::filesystem::path &path)
{
    addDiagnostic(diagnostics, std::move(code), std::move(fieldPath),
                  std::move(label) + " does not exist: " + pathMessage(path));
}

std::string characterDefinitionField(size_t fileIndex, size_t characterIndex, std::string_view suffix)
{
    std::string field = "characters/" + std::to_string(fileIndex) + "/characters/" + std::to_string(characterIndex);
    if (!suffix.empty()) {
        field += "/";
        field += suffix;
    }
    return field;
}

bool readCharacterHealth(const json &item, ScenarioCombatBridge &bridge)
{
    if (!item.is_object()) {
        return false;
    }
    const auto it = item.find("defaultHealth");
    if (it == item.end()) {
        return false;
    }

    const std::optional<uint32_t> maxHealth = readUnsignedField(*it, {"max", "maxHealth"});
    const std::optional<uint32_t> currentHealth = readUnsignedField(*it, {"current", "health"});
    if (!maxHealth.has_value() || !currentHealth.has_value() || *maxHealth == 0 ||
        *currentHealth > *maxHealth) {
        return false;
    }

    bridge.maxHealth = *maxHealth;
    bridge.health = *currentHealth;
    return true;
}

bool readCharacterHurtbox(const json &item, ScenarioHurtbox &hurtbox)
{
    if (!item.is_object()) {
        return false;
    }
    const auto it = item.find("defaultHurtbox");
    if (it == item.end() || !it->is_object()) {
        return false;
    }

    const std::string shape = it->value("shape", "box");
    const std::optional<CombatVolumeKind> kind = tryVolumeKindFromString(shape);
    const auto sizeIt = it->find("size");
    const auto offsetIt = it->find("offset");
    if (!kind.has_value() || sizeIt == it->end() || offsetIt == it->end()) {
        return false;
    }

    const std::optional<glm::vec3> size = readRequiredVec3(*sizeIt, true);
    const std::optional<glm::vec3> offset = readRequiredVec3(*offsetIt, false);
    if (!size.has_value() || !offset.has_value()) {
        return false;
    }

    hurtbox.kind = *kind;
    hurtbox.size = *size;
    hurtbox.offset = *offset;
    return true;
}

ScenarioCharacterLoadResult loadCharacterDefinitions(const CombatScenario &scenario,
                                                     const std::vector<std::filesystem::path> &resolvedPaths,
                                                     const std::unordered_set<std::string> &knownMoveIds)
{
    ScenarioCharacterLoadResult result;
    std::unordered_map<std::string, std::string> seenCharacterFields;

    for (size_t fileIndex = 0; fileIndex < scenario.characterPaths.size(); ++fileIndex) {
        const std::filesystem::path &resolvedPath = resolvedPaths[fileIndex];
        std::ifstream input{resolvedPath};
        if (!input) {
            addMissingFileDiagnostic(result.diagnostics, "missing_character_file",
                                     "characters/" + std::to_string(fileIndex), "Character definition file",
                                     resolvedPath);
            continue;
        }

        json document;
        input >> document;
        if (document.value("schemaVersion", 0u) != kCombatScenarioSchemaVersion) {
            addDiagnostic(result.diagnostics, "unsupported_character_schema_version",
                          "characters/" + std::to_string(fileIndex) + "/schemaVersion",
                          "Character definition schema version is not supported");
        }

        const auto charactersIt = document.find("characters");
        if (charactersIt == document.end() || !charactersIt->is_array() || charactersIt->empty()) {
            addDiagnostic(result.diagnostics, "missing_character_definitions",
                          "characters/" + std::to_string(fileIndex) + "/characters",
                          "Character definition file must contain at least one character");
            continue;
        }

        for (size_t characterIndex = 0; characterIndex < charactersIt->size(); ++characterIndex) {
            const json &item = charactersIt->at(characterIndex);
            const std::string baseField = characterDefinitionField(fileIndex, characterIndex, "");
            if (!item.is_object()) {
                addDiagnostic(result.diagnostics, "invalid_character_definition", baseField,
                              "Character definition entries must be objects");
                continue;
            }

            ScenarioCharacterDefinition definition;
            definition.sourcePath = scenario.characterPaths[fileIndex];
            definition.logicalId = item.value("id", "");
            definition.label = item.value("label", definition.logicalId);

            bool duplicate = false;
            if (definition.logicalId.empty()) {
                addDiagnostic(result.diagnostics, "missing_character_id",
                              characterDefinitionField(fileIndex, characterIndex, "id"),
                              "Character definition id is required");
            } else if (!seenCharacterFields.emplace(definition.logicalId, baseField).second) {
                duplicate = true;
                addDiagnostic(result.diagnostics, "duplicate_character_id",
                              characterDefinitionField(fileIndex, characterIndex, "id"),
                              "Character definition id is duplicated: " + definition.logicalId);
            }

            if (!readCharacterHealth(item, definition.combatBridge)) {
                addDiagnostic(result.diagnostics, "invalid_character_health",
                              characterDefinitionField(fileIndex, characterIndex, "defaultHealth"),
                              "Character default health must have positive max and current not exceeding max");
            }

            if (!readCharacterHurtbox(item, definition.hurtbox)) {
                addDiagnostic(result.diagnostics, "invalid_character_hurtbox",
                              characterDefinitionField(fileIndex, characterIndex, "defaultHurtbox"),
                              "Character default hurtbox must have a valid shape, positive size, and finite offset");
            }

            const auto movesIt = item.find("defaultMoves");
            if (movesIt == item.end() || !movesIt->is_array() || movesIt->empty()) {
                addDiagnostic(result.diagnostics, "missing_character_moves",
                              characterDefinitionField(fileIndex, characterIndex, "defaultMoves"),
                              "Character definition must list at least one default move id");
            } else {
                definition.defaultMoves.reserve(movesIt->size());
                for (size_t moveIndex = 0; moveIndex < movesIt->size(); ++moveIndex) {
                    const std::string moveField =
                        characterDefinitionField(fileIndex, characterIndex,
                                                 "defaultMoves/" + std::to_string(moveIndex));
                    if (!movesIt->at(moveIndex).is_string() || movesIt->at(moveIndex).get<std::string>().empty()) {
                        addDiagnostic(result.diagnostics, "invalid_character_move", moveField,
                                      "Character default move ids must be non-empty strings");
                        continue;
                    }

                    const std::string moveId = movesIt->at(moveIndex).get<std::string>();
                    if (!knownMoveIds.contains(moveId)) {
                        addDiagnostic(result.diagnostics, "unknown_character_move", moveField,
                                      "Character default move references an unknown scenario move: " + moveId);
                        continue;
                    }
                    definition.defaultMoves.push_back(moveId);
                }
            }

            if (!definition.logicalId.empty() && !duplicate) {
                result.characters.push_back(std::move(definition));
            }
        }
    }

    return result;
}

std::vector<ScenarioDiagnostic> validateCharacterReferences(
    const CombatScenario &scenario, const std::vector<ScenarioCharacterDefinition> &characters)
{
    std::vector<ScenarioDiagnostic> diagnostics;
    std::unordered_set<std::string> characterIds;
    for (const ScenarioCharacterDefinition &character : characters) {
        characterIds.insert(character.logicalId);
    }

    for (size_t i = 0; i < scenario.actors.size(); ++i) {
        const ScenarioActor &actor = scenario.actors[i];
        if (actor.characterId.empty()) {
            continue;
        }
        if (!characterIds.contains(actor.characterId)) {
            addDiagnostic(diagnostics, "unknown_actor_character", "actors/" + std::to_string(i) + "/character",
                          "Scenario actor references an unknown character: " + actor.characterId);
        }
    }

    return diagnostics;
}

void applyCharacterDefaults(CombatScenario &scenario, const std::vector<ScenarioCharacterDefinition> &characters)
{
    std::unordered_map<std::string, const ScenarioCharacterDefinition *> definitions;
    for (const ScenarioCharacterDefinition &character : characters) {
        definitions.emplace(character.logicalId, &character);
    }

    for (ScenarioActor &actor : scenario.actors) {
        if (actor.characterId.empty()) {
            continue;
        }

        const auto it = definitions.find(actor.characterId);
        if (it == definitions.end()) {
            continue;
        }

        const ScenarioCharacterDefinition &definition = *it->second;
        if (!actor.hasCombatBridgeOverride) {
            actor.combatBridge = definition.combatBridge;
        }
        if (!actor.hasHurtboxOverride) {
            actor.hurtbox = definition.hurtbox;
        }
    }
}

std::vector<ScenarioDiagnostic> validateScenario(const CombatScenario &scenario)
{
    std::vector<ScenarioDiagnostic> diagnostics;
    if (scenario.schemaVersion != kCombatScenarioSchemaVersion) {
        addDiagnostic(diagnostics, "unsupported_schema_version", "schemaVersion",
                      "Combat scenario schema version is not supported");
    }
    if (scenario.logicalId.empty()) {
        addDiagnostic(diagnostics, "missing_logical_id", "logicalId", "Scenario logicalId is required");
    }
    if (scenario.durationTicks == 0) {
        addDiagnostic(diagnostics, "invalid_duration", "durationTicks", "Scenario durationTicks must be positive");
    }
    if (scenario.mapPath.empty()) {
        addDiagnostic(diagnostics, "missing_map", "map", "Scenario map path is required");
    }
    if (scenario.movePaths.empty()) {
        addDiagnostic(diagnostics, "missing_moves", "moves", "Scenario must reference at least one move asset");
    }
    if (scenario.actors.empty()) {
        addDiagnostic(diagnostics, "missing_actors", "actors", "Scenario must define at least one actor");
    }
    if (scenario.goldenPath.empty()) {
        addDiagnostic(diagnostics, "missing_golden", "golden", "Scenario golden trace path is required");
    }

    std::unordered_set<uint32_t> actorIds;
    for (size_t i = 0; i < scenario.actors.size(); ++i) {
        const ScenarioActor &actor = scenario.actors[i];
        const std::string field = "actors/" + std::to_string(i);
        if (actor.id.value == 0 || !actorIds.insert(actor.id.value).second) {
            addDiagnostic(diagnostics, "invalid_actor_id", field + "/id", "Actor ids must be non-zero and unique");
        }
        if (actor.spawnId.empty() && !actor.translation.has_value()) {
            addDiagnostic(diagnostics, "missing_actor_placement", field,
                          "Actor must name a spawn or provide an explicit translation");
        }
        if (actor.team.empty() && actor.spawnId.empty()) {
            addDiagnostic(diagnostics, "missing_actor_team", field + "/team",
                          "Actor team is required when no spawn supplies ownership");
        }
        if (actor.combatBridge.maxHealth == 0 || actor.combatBridge.health > actor.combatBridge.maxHealth) {
            addDiagnostic(diagnostics, "invalid_combat_bridge", field + "/combatBridge",
                          "Scenario combat bridge health must be positive and not exceed maxHealth");
        }
        if (!isValidHurtbox(actor.hurtbox)) {
            addDiagnostic(diagnostics, "invalid_actor_hurtbox", field + "/hurtbox",
                          "Scenario actor hurtbox size must be positive and finite");
        }
    }

    for (size_t i = 0; i < scenario.inputs.size(); ++i) {
        const ScenarioInputCommand &input = scenario.inputs[i];
        const std::string field = "inputs/" + std::to_string(i);
        if (input.tick >= scenario.durationTicks) {
            addDiagnostic(diagnostics, "input_tick_out_of_range", field + "/tick",
                          "Input tick must be inside scenario duration");
        }
        if (!actorIds.contains(input.actorId.value)) {
            addDiagnostic(diagnostics, "unknown_input_actor", field + "/actor",
                          "Input references an unknown scenario actor");
        }
        if (input.command.empty()) {
            addDiagnostic(diagnostics, "missing_input_command", field + "/command", "Input command is required");
        }
    }

    return diagnostics;
}

std::vector<ScenarioDiagnostic> validateContentGraph(
    const CombatScenario &scenario, const std::vector<content::CompiledMove> &moves,
    const std::unordered_map<std::string, animation::ProxyAnimationAsset> &animations)
{
    std::vector<ScenarioDiagnostic> diagnostics;

    std::unordered_map<std::string, size_t> moveIndexes;
    for (size_t i = 0; i < moves.size(); ++i) {
        const content::CompiledMove &move = moves[i];
        if (!moveIndexes.emplace(move.logicalId, i).second) {
            addDiagnostic(diagnostics, "duplicate_move_logical_id", "moves/" + std::to_string(i),
                          "Move logicalId is duplicated in scenario content graph: " + move.logicalId);
        }
    }

    std::unordered_set<std::string> animationBindings;
    for (size_t i = 0; i < scenario.animations.size(); ++i) {
        const ScenarioAnimationBinding &binding = scenario.animations[i];
        const std::string field = "animations/" + std::to_string(i);
        if (!animationBindings.insert(binding.moveLogicalId).second) {
            addDiagnostic(diagnostics, "duplicate_animation_binding", field + "/move",
                          "Scenario has duplicate proxy animation binding for move: " + binding.moveLogicalId);
        }
        if (!moveIndexes.contains(binding.moveLogicalId)) {
            addDiagnostic(diagnostics, "unknown_animation_move", field + "/move",
                          "Proxy animation binding references an unknown move: " + binding.moveLogicalId);
        }
    }

    for (size_t moveIndex = 0; moveIndex < moves.size(); ++moveIndex) {
        const content::CompiledMove &move = moves[moveIndex];
        if (move.hitboxTracks.empty()) {
            continue;
        }

        const auto animationIt = animations.find(move.logicalId);
        if (animationIt == animations.end()) {
            addDiagnostic(diagnostics, "missing_move_animation", "moves/" + std::to_string(moveIndex),
                          "Move has hitbox tracks but no proxy animation binding: " + move.logicalId);
            continue;
        }

        std::unordered_set<std::string> socketNames;
        for (const animation::ProxySocketTrack &socket : animationIt->second.sockets) {
            socketNames.insert(socket.name);
        }

        for (const content::CompiledHitboxTrack &hitbox : move.hitboxTracks) {
            if (socketNames.contains(hitbox.socket)) {
                continue;
            }

            std::string trackId = std::to_string(hitbox.trackId);
            if (hitbox.trackId > 0 && hitbox.trackId <= move.internTable.trackIds.size()) {
                trackId = move.internTable.trackIds[hitbox.trackId - 1];
            }
            addDiagnostic(diagnostics, "missing_hitbox_socket",
                          "moves/" + move.logicalId + "/hitboxTracks/" + trackId + "/socket",
                          "Move hitbox references socket '" + hitbox.socket +
                              "' that is absent from proxy animation bound to move: " + move.logicalId);
        }
    }

    return diagnostics;
}

void appendByte(uint64_t &hash, uint8_t value)
{
    hash ^= value;
    hash *= kFnvPrime;
}

void appendUint32(uint64_t &hash, uint32_t value)
{
    for (int byteIndex = 0; byteIndex < 4; ++byteIndex) {
        appendByte(hash, static_cast<uint8_t>((value >> (byteIndex * 8)) & 0xffu));
    }
}

void appendUint64(uint64_t &hash, uint64_t value)
{
    for (int byteIndex = 0; byteIndex < 8; ++byteIndex) {
        appendByte(hash, static_cast<uint8_t>((value >> (byteIndex * 8)) & 0xffu));
    }
}

int32_t quantizeFloat(float value)
{
    if (!std::isfinite(value)) {
        return 0;
    }

    const double scaled = static_cast<double>(value) * 1000.0;
    const double clamped = std::clamp(scaled, static_cast<double>(std::numeric_limits<int32_t>::min()),
                                      static_cast<double>(std::numeric_limits<int32_t>::max()));
    return static_cast<int32_t>(std::llround(clamped));
}

void appendInt32(uint64_t &hash, int32_t value) { appendUint32(hash, static_cast<uint32_t>(value)); }

void appendString(uint64_t &hash, std::string_view value)
{
    appendUint32(hash, static_cast<uint32_t>(value.size()));
    for (char character : value) {
        appendByte(hash, static_cast<uint8_t>(character));
    }
}

std::string hashToString(uint64_t hash)
{
    std::ostringstream output;
    output << "0x" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

void appendTraceEvent(ScenarioTrace &trace, uint32_t tick, std::string kind, uint32_t actorId, uint32_t targetId,
                      std::string move, uint32_t moveTick, std::string label)
{
    trace.events.push_back({
        static_cast<uint32_t>(trace.events.size() + 1),
        tick,
        std::move(kind),
        actorId,
        targetId,
        std::move(move),
        moveTick,
        std::move(label),
    });
}

std::string eventKindToString(CombatEventKind kind)
{
    switch (kind) {
    case CombatEventKind::moveStarted:
        return "move_started";
    case CombatEventKind::phaseEntered:
        return "phase_entered";
    case CombatEventKind::authoredEvent:
        return "authored_event";
    case CombatEventKind::moveCompleted:
        return "move_completed";
    case CombatEventKind::moveCanceled:
        return "move_canceled";
    }
    return "combat_event";
}

void appendCombatEvents(ScenarioTrace &trace, const CombatTickResult &result, const CombatRuntime &runtime)
{
    for (const CombatEvent &event : result.events) {
        std::string moveLabel;
        if (const RuntimeMove *move = runtime.moveLibrary.findMove(event.moveId)) {
            moveLabel = move->compiled.logicalId;
        }
        appendTraceEvent(trace, event.tick, eventKindToString(event.kind), event.actorId.value, 0, moveLabel,
                         event.moveTick, event.label);
    }
}

const content::RuntimeSpawnPoint *findSpawn(const content::RuntimeWorld &world, std::string_view spawnId)
{
    const auto it = std::find_if(world.spawnPoints.begin(), world.spawnPoints.end(),
                                 [spawnId](const content::RuntimeSpawnPoint &spawn) { return spawn.id == spawnId; });
    return it == world.spawnPoints.end() ? nullptr : &*it;
}

simulation::RuntimeWorld makeRuntimeWorld(const CombatScenario &scenario, const content::RuntimeWorld &contentWorld)
{
    simulation::RuntimeWorld world = simulation::importRuntimeWorld(contentWorld);
    world.actors.clear();

    for (const ScenarioActor &scenarioActor : scenario.actors) {
        simulation::RuntimeActor actor;
        actor.id = scenarioActor.id;
        actor.spawnId = scenarioActor.spawnId;
        actor.team = scenarioActor.team;

        if (!scenarioActor.spawnId.empty()) {
            if (const content::RuntimeSpawnPoint *spawn = findSpawn(contentWorld, scenarioActor.spawnId)) {
                actor.team = scenarioActor.team.empty() ? spawn->team : scenarioActor.team;
                actor.state.currentTransform.translation = spawn->translation;
                actor.state.currentTransform.rotationDegrees.y = spawn->yawDegrees;
            }
        }

        if (scenarioActor.translation.has_value()) {
            actor.state.currentTransform.translation = *scenarioActor.translation;
        }
        if (scenarioActor.yawDegrees.has_value()) {
            actor.state.currentTransform.rotationDegrees.y = *scenarioActor.yawDegrees;
        }

        actor.state.previousTransform = actor.state.currentTransform;
        actor.state.moveSpeedWorldUnitsPerSecond = world.tuning.playerRunSpeedWorldUnitsPerSecond;
        world.actors.push_back(std::move(actor));
    }

    std::sort(world.actors.begin(), world.actors.end(),
              [](const simulation::RuntimeActor &left, const simulation::RuntimeActor &right) {
                  return left.id.value < right.id.value;
              });
    return world;
}

ResolvedVolume hurtVolumeForActor(const ScenarioActor &scenarioActor, const simulation::RuntimeActor &actor)
{
    ResolvedVolume volume;
    volume.kind = scenarioActor.hurtbox.kind;
    volume.center = actor.state.currentTransform.translation + scenarioActor.hurtbox.offset;
    volume.halfExtents = scenarioActor.hurtbox.size * 0.5f;
    volume.radius = scenarioActor.hurtbox.size.x * 0.5f;
    volume.halfHeight = scenarioActor.hurtbox.size.y * 0.5f;
    return volume;
}

CombatVolume volumeFromHitbox(const content::CompiledHitboxTrack &track, float actorYawDegrees)
{
    CombatVolume volume;
    volume.kind = volumeKindFromString(track.shape);
    volume.binding.socketName = track.socket;
    volume.binding.localOffset = rotateYaw(track.offset, actorYawDegrees);
    volume.halfExtents = track.size * 0.5f;
    volume.radius = track.size.x * 0.5f;
    volume.halfHeight = track.size.y * 0.5f;
    return volume;
}

std::vector<SocketTransform> actorWorldSockets(const animation::ProxyPose &pose,
                                               const simulation::RuntimeActor &actor)
{
    std::vector<SocketTransform> sockets;
    sockets.reserve(pose.sockets.size());
    const glm::vec3 actorTranslation = actor.state.currentTransform.translation;
    const float actorYaw = actor.state.currentTransform.rotationDegrees.y;
    for (const animation::SocketPose &socket : pose.sockets) {
        sockets.push_back({socket.name, actorTranslation + rotateYaw(socket.worldTranslation, actorYaw)});
    }
    return sockets;
}

glm::vec3 actorWorldRoot(const animation::ProxyPose &pose, const simulation::RuntimeActor &actor)
{
    return actor.state.currentTransform.translation +
           rotateYaw(pose.rootTranslation, actor.state.currentTransform.rotationDegrees.y);
}

bool moveHasActiveTag(const RuntimeMove &move, uint32_t moveTick, std::string_view tag)
{
    for (const content::CompiledPhase &phase : move.compiled.phases) {
        if (!isActiveOnTick(phase.range, moveTick)) {
            continue;
        }
        for (uint32_t tagId : phase.tagIds) {
            if (tagId > 0 && tagId <= move.compiled.internTable.tagIds.size() &&
                move.compiled.internTable.tagIds[tagId - 1] == tag) {
                return true;
            }
        }
    }
    return false;
}

bool actorIsInvulnerable(const CombatRuntime &runtime, simulation::RuntimeActorId actorId)
{
    const CombatActorState *actor = findCombatActor(runtime, actorId);
    if (actor == nullptr || actor->activeMoveId == 0) {
        return false;
    }
    const RuntimeMove *move = runtime.moveLibrary.findMove(actor->activeMoveId);
    return move != nullptr && moveHasActiveTag(*move, actor->moveTick, "invulnerable");
}

std::vector<simulation::InputFrame> inputsForTick(const CombatScenario &scenario, uint32_t tick)
{
    std::vector<simulation::InputFrame> inputs;
    for (const ScenarioInputCommand &command : scenario.inputs) {
        if (command.tick != tick) {
            continue;
        }
        simulation::InputFrame input;
        input.tick = tick;
        input.actorId = command.actorId;
        input.pressedCommands.push_back(command.command);
        inputs.push_back(std::move(input));
    }
    std::sort(inputs.begin(), inputs.end(), [](const simulation::InputFrame &left,
                                               const simulation::InputFrame &right) {
        if (left.actorId.value != right.actorId.value) {
            return left.actorId.value < right.actorId.value;
        }
        return left.pressedCommands < right.pressedCommands;
    });
    return inputs;
}

const ScenarioActor *findScenarioActor(const CombatScenario &scenario, simulation::RuntimeActorId actorId)
{
    const auto it = std::find_if(scenario.actors.begin(), scenario.actors.end(),
                                 [actorId](const ScenarioActor &actor) { return actor.id.value == actorId.value; });
    return it == scenario.actors.end() ? nullptr : &*it;
}

const animation::ProxyAnimationAsset *findAnimationForMove(
    const std::unordered_map<std::string, animation::ProxyAnimationAsset> &animations, std::string_view moveId)
{
    const auto it = animations.find(std::string{moveId});
    return it == animations.end() ? nullptr : &it->second;
}

std::vector<CombatTickContext> collectCollisionContexts(
    CombatRuntime &runtime, const CombatScenario &scenario, const simulation::RuntimeWorld &world,
    const std::unordered_map<std::string, animation::ProxyAnimationAsset> &animations,
    std::vector<ScenarioTraceEvent> &pendingTraceEvents)
{
    std::vector<CombatTickContext> contexts;

    for (CombatActorState &actorState : runtime.actors) {
        if (actorState.activeMoveId == 0) {
            continue;
        }

        const RuntimeMove *move = runtime.moveLibrary.findMove(actorState.activeMoveId);
        const simulation::RuntimeActor *attacker = simulation::findActor(world, actorState.actorId);
        if (move == nullptr || attacker == nullptr) {
            continue;
        }

        const animation::ProxyAnimationAsset *animation = findAnimationForMove(animations, move->compiled.logicalId);
        const bool hadPriorHit = !actorState.hitRegistry.empty();
        bool activeHitboxThisTick = false;
        bool landedHitThisTick = false;
        bool blockedHitThisTick = false;

        for (const content::CompiledHitboxTrack &hitbox : move->compiled.hitboxTracks) {
            if (!isActiveOnTick(hitbox.range, actorState.moveTick)) {
                continue;
            }
            activeHitboxThisTick = true;
            if (animation == nullptr) {
                continue;
            }

            const uint32_t previousMoveTick = actorState.moveTick == 0 ? 0 : actorState.moveTick - 1;
            const animation::ProxyPose previousPose = animation::sampleProxyPoseAtTick(*animation, previousMoveTick);
            const animation::ProxyPose currentPose = animation::sampleProxyPoseAtTick(*animation, actorState.moveTick);
            const CombatVolume hitVolume = volumeFromHitbox(hitbox, attacker->state.currentTransform.rotationDegrees.y);
            const ResolvedVolume previousVolume =
                resolveVolume(hitVolume, actorWorldRoot(previousPose, *attacker), actorWorldSockets(previousPose, *attacker));
            const ResolvedVolume currentVolume =
                resolveVolume(hitVolume, actorWorldRoot(currentPose, *attacker), actorWorldSockets(currentPose, *attacker));

            std::vector<HurtVolume> targets;
            for (const ScenarioActor &scenarioActor : scenario.actors) {
                if (scenarioActor.id.value == actorState.actorId.value) {
                    continue;
                }
                const simulation::RuntimeActor *target = simulation::findActor(world, scenarioActor.id);
                if (target == nullptr || target->team == attacker->team) {
                    continue;
                }

                const ResolvedVolume hurtVolume = hurtVolumeForActor(scenarioActor, *target);
                const bool geometricHit = sweptIntersects(previousVolume, currentVolume, hurtVolume);
                const bool invulnerable = actorIsInvulnerable(runtime, scenarioActor.id);
                if (geometricHit && invulnerable) {
                    blockedHitThisTick = true;
                    CombatHitEffectResult blockedEffect;
                    if (const CombatActorState *blockedActor = findCombatActor(runtime, scenarioActor.id)) {
                        blockedEffect.remainingHealth = blockedActor->currentHealth;
                    }
                    ScenarioTraceEvent blockedEvent;
                    blockedEvent.tick = runtime.tick;
                    blockedEvent.kind = "hit_blocked";
                    blockedEvent.actorId = actorState.actorId.value;
                    blockedEvent.targetId = scenarioActor.id.value;
                    blockedEvent.move = move->compiled.logicalId;
                    blockedEvent.moveTick = actorState.moveTick;
                    blockedEvent.label = "invulnerable:" + move->compiled.internTable.trackIds[hitbox.trackId - 1];
                    blockedEvent.hasEffect = true;
                    blockedEvent.damage = 0;
                    blockedEvent.targetRemainingHealth = blockedEffect.remainingHealth;
                    pendingTraceEvents.push_back(std::move(blockedEvent));
                }

                targets.push_back({
                    scenarioActor.id.value,
                    hurtVolume,
                    invulnerable ? std::vector<uint32_t>{kScenarioInvulnerableTag} : std::vector<uint32_t>{},
                });
            }

            CombatHitQuery query;
            query.tick = actorState.moveTick;
            query.activeRange = hitbox.range;
            query.attackerId = actorState.actorId.value;
            query.moveInstanceSequence = actorState.moveInstanceSequence;
            query.hitboxTrackId = hitbox.trackId;
            query.previousHitVolume = previousVolume;
            query.currentHitVolume = currentVolume;
            query.swept = true;
            query.blockedByTargetTags = {kScenarioInvulnerableTag};
            query.targets = std::move(targets);

            const std::vector<CombatHitCandidate> candidates = collectHitCandidates(query);
            const std::vector<CombatHitCandidate> filtered = filterOncePerTarget(candidates, actorState.hitRegistry);
            for (const CombatHitCandidate &candidate : filtered) {
                landedHitThisTick = true;
                const std::string &trackName = move->compiled.internTable.trackIds[hitbox.trackId - 1];
                CombatHitEffectResult effect;
                if (CombatActorState *victim = findCombatActor(runtime, {candidate.victimId})) {
                    effect = applyHitEffect(runtime, *victim, hitbox);
                }
                ScenarioTraceEvent hitEvent;
                hitEvent.tick = runtime.tick;
                hitEvent.kind = "hit";
                hitEvent.actorId = actorState.actorId.value;
                hitEvent.targetId = candidate.victimId;
                hitEvent.move = move->compiled.logicalId;
                hitEvent.moveTick = actorState.moveTick;
                hitEvent.label = trackName;
                hitEvent.hasEffect = true;
                hitEvent.damage = effect.damageApplied;
                hitEvent.targetRemainingHealth = effect.remainingHealth;
                hitEvent.reactionMove = effect.reactionStarted ? effect.reactionMove : std::string{};
                hitEvent.hitstopTicks = effect.hitstopTicks;
                hitEvent.stunTicks = effect.stunTicks;
                pendingTraceEvents.push_back(std::move(hitEvent));
            }
        }

        CombatTickContext context;
        context.actorId = actorState.actorId;
        if (landedHitThisTick || hadPriorHit) {
            context.cancelSignal = CancelSignal::hit;
        } else if (activeHitboxThisTick && !blockedHitThisTick) {
            context.cancelSignal = CancelSignal::whiff;
            pendingTraceEvents.push_back({0,
                                          runtime.tick,
                                          "whiff",
                                          actorState.actorId.value,
                                          0,
                                          move->compiled.logicalId,
                                          actorState.moveTick,
                                          "active_hitbox"});
        } else {
            context.cancelSignal = CancelSignal::none;
        }
        contexts.push_back(context);
    }

    std::sort(contexts.begin(), contexts.end(), [](const CombatTickContext &left, const CombatTickContext &right) {
        return left.actorId.value < right.actorId.value;
    });
    return contexts;
}

uint64_t finalStateHash(const simulation::RuntimeWorld &world, const CombatRuntime &runtime)
{
    uint64_t hash = kFnvOffset;
    appendUint64(hash, simulation::stateHash(world));
    appendUint32(hash, runtime.tick);

    std::vector<const CombatActorState *> actors;
    actors.reserve(runtime.actors.size());
    for (const CombatActorState &actor : runtime.actors) {
        actors.push_back(&actor);
    }
    std::sort(actors.begin(), actors.end(), [](const CombatActorState *left, const CombatActorState *right) {
        return left->actorId.value < right->actorId.value;
    });

    for (const CombatActorState *actor : actors) {
        appendUint32(hash, actor->actorId.value);
        appendUint32(hash, actor->activeMoveId);
        appendUint32(hash, actor->moveInstanceSequence);
        appendUint32(hash, actor->moveTick);
        appendUint32(hash, actor->maxHealth);
        appendUint32(hash, actor->currentHealth);
        appendUint32(hash, actor->hitstopRemaining);
        appendUint32(hash, actor->stunRemaining);
        appendUint32(hash, static_cast<uint32_t>(actor->hitRegistry.size()));
        for (uint32_t hit : actor->hitRegistry) {
            appendUint32(hash, hit);
        }
    }
    return hash;
}

ScenarioTrace traceFromJson(const json &document)
{
    ScenarioTrace trace;
    trace.scenarioId = document.value("scenarioId", "");
    trace.seed = document.value("seed", 0ull);
    trace.ticksRun = document.value("ticksRun", 0u);
    trace.finalStateHash = document.value("finalStateHash", "");
    for (const json &item : document.value("events", json::array())) {
        trace.events.push_back({
            item.value("sequence", 0u),
            item.value("tick", 0u),
            item.value("kind", ""),
            item.value("actor", 0u),
            item.value("target", 0u),
            item.value("move", ""),
            item.value("moveTick", 0u),
            item.value("label", ""),
            item.contains("damage") || item.contains("targetRemainingHealth") || item.contains("reactionMove") ||
                item.contains("hitstopTicks") || item.contains("stunTicks"),
            item.value("damage", 0u),
            item.value("targetRemainingHealth", 0u),
            item.value("reactionMove", ""),
            static_cast<uint16_t>(item.value("hitstopTicks", 0u)),
            static_cast<uint16_t>(item.value("stunTicks", 0u)),
        });
    }
    return trace;
}

bool tracesEqual(const ScenarioTrace &left, const ScenarioTrace &right)
{
    return toJson(left) == toJson(right);
}

std::string traceMismatchMessage(const ScenarioTrace &actual, const ScenarioTrace &expected)
{
    const size_t sharedEventCount = std::min(actual.events.size(), expected.events.size());
    for (size_t i = 0; i < sharedEventCount; ++i) {
        if (toJson(actual.events[i]) == toJson(expected.events[i])) {
            continue;
        }

        return "First mismatch at event " + std::to_string(i + 1) + " near tick " +
               std::to_string(std::min(actual.events[i].tick, expected.events[i].tick)) + ". expected=" +
               toJson(expected.events[i]).dump() + " actual=" + toJson(actual.events[i]).dump();
    }

    if (actual.events.size() != expected.events.size()) {
        return "Event count mismatch. expected=" + std::to_string(expected.events.size()) +
               " actual=" + std::to_string(actual.events.size());
    }

    if (actual.finalStateHash != expected.finalStateHash) {
        return "Final state hash mismatch. expected=" + expected.finalStateHash +
               " actual=" + actual.finalStateHash;
    }

    return "Trace metadata mismatch. expected=" + toJson(expected).dump() + " actual=" + toJson(actual).dump();
}

void writeJsonFileAtomically(const std::filesystem::path &path, const json &document)
{
    if (path.empty()) {
        return;
    }

    const std::filesystem::path absolutePath = std::filesystem::absolute(path);
    std::filesystem::create_directories(absolutePath.parent_path());
    const std::filesystem::path temporaryPath = absolutePath.string() + ".tmp";
    {
        std::ofstream output{temporaryPath, std::ios::trunc};
        if (!output) {
            throw std::runtime_error("Could not open JSON output '" + temporaryPath.string() + "'");
        }
        output << document.dump(2) << '\n';
    }
    std::error_code error;
    std::filesystem::remove(absolutePath, error);
    error.clear();
    std::filesystem::rename(temporaryPath, absolutePath, error);
    if (error) {
        throw std::runtime_error("Could not replace JSON output '" + absolutePath.string() + "': " + error.message());
    }
}
} // namespace

CombatScenario combatScenarioFromJson(const nlohmann::json &document, std::filesystem::path sourcePath)
{
    CombatScenario scenario;
    scenario.sourcePath = std::move(sourcePath);
    scenario.schemaVersion = document.value("schemaVersion", 0u);
    scenario.logicalId = document.value("logicalId", "");
    scenario.durationTicks = document.value("durationTicks", 0u);
    scenario.seed = document.value("seed", 0ull);
    scenario.mapPath = document.value("map", "");
    scenario.goldenPath = document.value("golden", "");

    for (const json &item : document.value("characters", json::array())) {
        scenario.characterPaths.emplace_back(item.get<std::string>());
    }
    for (const json &item : document.value("moves", json::array())) {
        scenario.movePaths.emplace_back(item.get<std::string>());
    }
    for (const json &item : document.value("animations", json::array())) {
        scenario.animations.push_back({
            item.value("move", ""),
            item.value("path", ""),
        });
    }
    for (const json &item : document.value("actors", json::array())) {
        ScenarioActor actor;
        actor.id = {item.value("id", 0u)};
        actor.characterId = item.value("character", "");
        actor.spawnId = item.value("spawn", "");
        actor.team = item.value("team", "");
        if (item.contains("translation")) {
            actor.translation = readVec3(item.at("translation"), {0.0f, 0.0f, 0.0f});
        }
        if (item.contains("yawDegrees")) {
            actor.yawDegrees = item.at("yawDegrees").get<float>();
        }
        if (item.contains("hurtbox") && item.at("hurtbox").is_object()) {
            actor.hasHurtboxOverride = true;
            actor.hurtbox = readHurtbox(item.at("hurtbox"), actor.hurtbox);
        }
        if (item.contains("combatBridge")) {
            actor.hasCombatBridgeOverride = true;
            actor.combatBridge = readCombatBridge(item.value("combatBridge", json::object()));
        }
        scenario.actors.push_back(std::move(actor));
    }
    for (const json &item : document.value("inputs", json::array())) {
        scenario.inputs.push_back({
            item.value("tick", 0u),
            {item.value("actor", 0u)},
            item.value("command", ""),
        });
    }

    return scenario;
}

CombatScenario loadCombatScenario(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open combat scenario '" + path.string() + "'");
    }

    json document;
    input >> document;
    return combatScenarioFromJson(document, path);
}

std::filesystem::path resolveScenarioPath(const CombatScenario &scenario, const std::filesystem::path &path)
{
    return resolvePath(scenario, path);
}

CombatScenarioResolvedPaths resolveCombatScenarioPaths(const CombatScenario &scenario)
{
    CombatScenarioResolvedPaths paths;
    paths.mapPath = resolveScenarioPath(scenario, scenario.mapPath);
    paths.goldenPath = resolveScenarioPath(scenario, scenario.goldenPath);
    paths.characterPaths.reserve(scenario.characterPaths.size());
    for (const std::filesystem::path &characterPath : scenario.characterPaths) {
        paths.characterPaths.push_back(resolveScenarioPath(scenario, characterPath));
    }
    paths.movePaths.reserve(scenario.movePaths.size());
    for (const std::filesystem::path &movePath : scenario.movePaths) {
        paths.movePaths.push_back(resolveScenarioPath(scenario, movePath));
    }
    paths.animations.reserve(scenario.animations.size());
    for (const ScenarioAnimationBinding &binding : scenario.animations) {
        paths.animations.push_back({binding.moveLogicalId, resolveScenarioPath(scenario, binding.path)});
    }
    return paths;
}

simulation::RuntimeWorld makeScenarioRuntimeWorld(const CombatScenario &scenario,
                                                  const content::RuntimeWorld &contentWorld)
{
    return makeRuntimeWorld(scenario, contentWorld);
}

nlohmann::json toJson(const ScenarioTraceEvent &event)
{
    json document = {
        {"actor", event.actorId},       {"kind", event.kind},       {"label", event.label},
        {"move", event.move},           {"moveTick", event.moveTick}, {"sequence", event.sequence},
        {"target", event.targetId},     {"tick", event.tick},
    };
    if (event.hasEffect) {
        document["damage"] = event.damage;
        document["hitstopTicks"] = event.hitstopTicks;
        document["reactionMove"] = event.reactionMove;
        document["stunTicks"] = event.stunTicks;
        document["targetRemainingHealth"] = event.targetRemainingHealth;
    }
    return document;
}

nlohmann::json toJson(const ScenarioTrace &trace)
{
    json events = json::array();
    for (const ScenarioTraceEvent &event : trace.events) {
        events.push_back(toJson(event));
    }
    return {
        {"events", std::move(events)},
        {"finalStateHash", trace.finalStateHash},
        {"scenarioId", trace.scenarioId},
        {"seed", trace.seed},
        {"ticksRun", trace.ticksRun},
    };
}

nlohmann::json toJson(const ScenarioRunResult &result)
{
    json diagnostics = json::array();
    for (const ScenarioDiagnostic &diagnostic : result.diagnostics) {
        diagnostics.push_back({
            {"code", diagnostic.code},
            {"fieldPath", diagnostic.fieldPath},
            {"message", diagnostic.message},
            {"severity", diagnostic.severity},
        });
    }

    return {
        {"diagnostics", std::move(diagnostics)},
        {"golden", result.goldenPath.generic_string()},
        {"goldenCompared", result.goldenCompared},
        {"goldenMatched", result.goldenMatched},
        {"host", "combat_scenario_runner"},
        {"message", result.message},
        {"scenarioId", result.trace.scenarioId.empty() ? result.scenario.logicalId : result.trace.scenarioId},
        {"status", result.status},
        {"trace", toJson(result.trace)},
    };
}

ScenarioRunResult runCombatScenario(CombatScenario scenario, const ScenarioRunOptions &options)
{
    ScenarioRunResult result;
    result.scenario = scenario;
    if (options.overrideTicks.has_value()) {
        result.scenario.durationTicks = *options.overrideTicks;
    }
    if (options.overrideSeed.has_value()) {
        result.scenario.seed = *options.overrideSeed;
    }

    result.trace.scenarioId = result.scenario.logicalId;
    result.trace.seed = result.scenario.seed;
    result.goldenPath = resolvePath(result.scenario, result.scenario.goldenPath);

    try {
        result.diagnostics = validateScenario(result.scenario);
        if (!result.diagnostics.empty()) {
            result.status = "error";
            result.message = "Scenario validation failed";
            return result;
        }

        const std::filesystem::path resolvedMapPath = resolvePath(result.scenario, result.scenario.mapPath);
        std::vector<std::filesystem::path> resolvedCharacterPaths;
        resolvedCharacterPaths.reserve(result.scenario.characterPaths.size());
        std::vector<std::filesystem::path> resolvedMovePaths;
        resolvedMovePaths.reserve(result.scenario.movePaths.size());
        std::vector<std::filesystem::path> resolvedAnimationPaths;
        resolvedAnimationPaths.reserve(result.scenario.animations.size());

        if (!fileExists(resolvedMapPath)) {
            addMissingFileDiagnostic(result.diagnostics, "missing_map_file", "map", "Scenario map", resolvedMapPath);
        }
        for (size_t i = 0; i < result.scenario.characterPaths.size(); ++i) {
            const std::filesystem::path resolvedPath = resolvePath(result.scenario, result.scenario.characterPaths[i]);
            resolvedCharacterPaths.push_back(resolvedPath);
            if (!fileExists(resolvedPath)) {
                addMissingFileDiagnostic(result.diagnostics, "missing_character_file",
                                         "characters/" + std::to_string(i), "Character definition file",
                                         resolvedPath);
            }
        }
        for (size_t i = 0; i < result.scenario.movePaths.size(); ++i) {
            const std::filesystem::path resolvedPath = resolvePath(result.scenario, result.scenario.movePaths[i]);
            resolvedMovePaths.push_back(resolvedPath);
            if (!fileExists(resolvedPath)) {
                addMissingFileDiagnostic(result.diagnostics, "missing_move_file", "moves/" + std::to_string(i),
                                         "Scenario move asset", resolvedPath);
            }
        }
        for (size_t i = 0; i < result.scenario.animations.size(); ++i) {
            const std::filesystem::path resolvedPath = resolvePath(result.scenario, result.scenario.animations[i].path);
            resolvedAnimationPaths.push_back(resolvedPath);
            if (!fileExists(resolvedPath)) {
                addMissingFileDiagnostic(result.diagnostics, "missing_proxy_animation_file",
                                         "animations/" + std::to_string(i) + "/path",
                                         "Scenario proxy animation", resolvedPath);
            }
        }
        if (!options.updateGolden && !fileExists(result.goldenPath)) {
            addMissingFileDiagnostic(result.diagnostics, "missing_golden_file", "golden", "Scenario golden trace",
                                     result.goldenPath);
        }
        if (!result.diagnostics.empty()) {
            result.status = "error";
            result.message = "Scenario content graph validation failed";
            return result;
        }

        const content::AuthoringScene scene = content::loadAuthoringScene(resolvedMapPath);
        const content::CompileResult compiledWorld = content::compileRuntimeWorld(scene);
        if (!compiledWorld.ok()) {
            result.status = "error";
            result.message = "Map validation failed";
            for (const content::ContentDiagnostic &diagnostic : compiledWorld.validation.diagnostics) {
                addDiagnostic(result.diagnostics, diagnostic.code, diagnostic.fieldPath, diagnostic.message);
            }
            return result;
        }

        std::vector<content::CompiledMove> compiledMoves;
        compiledMoves.reserve(resolvedMovePaths.size());
        for (const std::filesystem::path &movePath : resolvedMovePaths) {
            const content::MoveAsset move = content::loadMoveAsset(movePath);
            const content::MoveCompileResult compiledMove = content::compileMoveAsset(move);
            if (!compiledMove.ok()) {
                result.status = "error";
                result.message = "Move validation failed";
                for (const content::ContentDiagnostic &diagnostic : compiledMove.validation.diagnostics) {
                    addDiagnostic(result.diagnostics, diagnostic.code, diagnostic.fieldPath, diagnostic.message);
                }
                return result;
            }
            compiledMoves.push_back(compiledMove.move);
        }

        std::unordered_set<std::string> knownMoveIds;
        for (const content::CompiledMove &move : compiledMoves) {
            knownMoveIds.insert(move.logicalId);
        }

        ScenarioCharacterLoadResult characterLoad =
            loadCharacterDefinitions(result.scenario, resolvedCharacterPaths, knownMoveIds);
        result.diagnostics = std::move(characterLoad.diagnostics);
        std::vector<ScenarioDiagnostic> characterReferenceDiagnostics =
            validateCharacterReferences(result.scenario, characterLoad.characters);
        result.diagnostics.insert(result.diagnostics.end(), characterReferenceDiagnostics.begin(),
                                  characterReferenceDiagnostics.end());
        if (!result.diagnostics.empty()) {
            result.status = "error";
            result.message = "Character definition validation failed";
            return result;
        }

        applyCharacterDefaults(result.scenario, characterLoad.characters);

        std::unordered_map<std::string, animation::ProxyAnimationAsset> animations;
        for (size_t i = 0; i < result.scenario.animations.size(); ++i) {
            const ScenarioAnimationBinding &binding = result.scenario.animations[i];
            animation::ProxyAnimationAsset asset = animation::loadProxyAnimation(resolvedAnimationPaths[i]);
            const content::ValidationResult validation = animation::validateProxyAnimation(asset);
            if (!validation.ok()) {
                result.status = "error";
                result.message = "Proxy animation validation failed";
                for (const content::ContentDiagnostic &diagnostic : validation.diagnostics) {
                    addDiagnostic(result.diagnostics, diagnostic.code, diagnostic.fieldPath, diagnostic.message);
                }
                return result;
            }
            animations[binding.moveLogicalId] = std::move(asset);
        }

        result.diagnostics = validateContentGraph(result.scenario, compiledMoves, animations);
        if (!result.diagnostics.empty()) {
            result.status = "error";
            result.message = "Scenario content graph validation failed";
            return result;
        }

        simulation::RuntimeWorld world = makeRuntimeWorld(result.scenario, compiledWorld.world);
        CombatRuntime runtime = createCombatRuntime(CombatMoveLibrary{std::move(compiledMoves)}, world.actors);
        for (const ScenarioActor &scenarioActor : result.scenario.actors) {
            if (CombatActorState *actor = findCombatActor(runtime, scenarioActor.id)) {
                setActorHealth(*actor, scenarioActor.combatBridge.health, scenarioActor.combatBridge.maxHealth);
            }
        }

        for (uint32_t tick = 0; tick < result.scenario.durationTicks; ++tick) {
            const std::vector<simulation::InputFrame> inputs = inputsForTick(result.scenario, tick);
            std::vector<ScenarioTraceEvent> pendingCollisionEvents;
            const std::vector<CombatTickContext> contexts =
                collectCollisionContexts(runtime, result.scenario, world, animations, pendingCollisionEvents);
            const CombatTickResult combatResult = advanceCombatTick(runtime, inputs, contexts);
            appendCombatEvents(result.trace, combatResult, runtime);
            for (ScenarioTraceEvent event : pendingCollisionEvents) {
                event.sequence = static_cast<uint32_t>(result.trace.events.size() + 1);
                result.trace.events.push_back(std::move(event));
            }
            simulation::advanceFixedTick(world, inputs);
        }

        result.trace.ticksRun = result.scenario.durationTicks;
        result.trace.finalStateHash = hashToString(finalStateHash(world, runtime));

        if (options.updateGolden) {
            if (!options.allowGoldenUpdate) {
                result.status = "error";
                result.message = "Golden update requires --update-golden and VAC_UPDATE_GOLDENS=1";
                return result;
            }
            writeGoldenTraceFile(result.goldenPath, result.trace);
            result.goldenCompared = true;
            result.goldenMatched = true;
            result.message = "Golden trace updated";
            return result;
        }

        const ScenarioTrace golden = loadGoldenTraceFile(result.goldenPath);
        result.goldenCompared = true;
        result.goldenMatched = tracesEqual(result.trace, golden);
        if (!result.goldenMatched) {
            result.status = "error";
            result.message = "Golden trace mismatch";
            addDiagnostic(result.diagnostics, "golden_mismatch", "golden",
                          traceMismatchMessage(result.trace, golden));
            return result;
        }

        result.message = "Scenario matched golden trace";
        return result;
    } catch (const std::exception &error) {
        result.status = "error";
        result.message = error.what();
        return result;
    }
}

void writeScenarioResultFile(const std::filesystem::path &path, const ScenarioRunResult &result)
{
    writeJsonFileAtomically(path, toJson(result));
}

void writeGoldenTraceFile(const std::filesystem::path &path, const ScenarioTrace &trace)
{
    writeJsonFileAtomically(path, toJson(trace));
}

ScenarioTrace loadGoldenTraceFile(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open golden trace '" + path.string() + "'");
    }

    json document;
    input >> document;
    return traceFromJson(document);
}
} // namespace vac::combat
