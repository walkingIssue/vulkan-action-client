#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "content/authoring_scene.hpp"

namespace vac::content
{
inline constexpr uint32_t kMoveAssetSchemaVersion = 1;

struct TickRange
{
    uint32_t begin = 0;
    uint32_t end = 0;
};

struct InputTrigger
{
    std::string command;
    uint32_t bufferTicks = 0;
};

struct MovePhase
{
    std::string id;
    TickRange range;
    std::vector<std::string> tags;
};

struct MovementTrack
{
    std::string id;
    TickRange range;
    glm::vec3 translation{0.0f};
    std::vector<std::string> tags;
};

struct HitboxTrack
{
    std::string id;
    TickRange range;
    std::string shape;
    std::string socket;
    glm::vec3 size{1.0f};
    glm::vec3 offset{0.0f};
    std::vector<std::string> tags;
};

struct HurtboxOverride
{
    std::string id;
    TickRange range;
    std::string profile;
    std::vector<std::string> tags;
};

struct CancelWindow
{
    std::string id;
    TickRange range;
    std::string condition;
    std::vector<std::string> destinations;
    std::vector<std::string> tags;
};

struct MoveEvent
{
    std::string id;
    uint32_t tick = 0;
    std::string type;
    std::string payload;
    std::vector<std::string> tags;
};

struct MoveResources
{
    float staminaCost = 0.0f;
};

struct MoveAsset
{
    std::filesystem::path sourcePath;
    uint32_t schemaVersion = kMoveAssetSchemaVersion;
    std::string assetGuid;
    std::string logicalId;
    uint32_t durationTicks = 0;
    std::vector<std::string> tags;
    std::vector<std::string> knownMoves;
    InputTrigger input;
    std::vector<MovePhase> phases;
    std::vector<MovementTrack> movementTracks;
    std::vector<HitboxTrack> hitboxTracks;
    std::vector<HurtboxOverride> hurtboxOverrides;
    std::vector<CancelWindow> cancels;
    MoveResources resources;
    std::vector<MoveEvent> events;
};

struct InternTable
{
    std::vector<std::string> moveIds;
    std::vector<std::string> tagIds;
    std::vector<std::string> eventIds;
    std::vector<std::string> trackIds;
};

struct CompiledRange
{
    uint32_t begin = 0;
    uint32_t end = 0;
};

struct CompiledPhase
{
    std::string id;
    CompiledRange range;
    std::vector<uint32_t> tagIds;
};

struct CompiledMovementTrack
{
    uint32_t trackId = 0;
    CompiledRange range;
    glm::vec3 translation{0.0f};
    std::vector<uint32_t> tagIds;
};

struct CompiledHitboxTrack
{
    uint32_t trackId = 0;
    CompiledRange range;
    std::string shape;
    std::string socket;
    glm::vec3 size{1.0f};
    glm::vec3 offset{0.0f};
    std::vector<uint32_t> tagIds;
};

struct CompiledHurtboxOverride
{
    uint32_t trackId = 0;
    CompiledRange range;
    std::string profile;
    std::vector<uint32_t> tagIds;
};

struct CompiledCancelWindow
{
    std::string id;
    CompiledRange range;
    std::string condition;
    std::vector<uint32_t> destinationMoveIds;
    std::vector<uint32_t> tagIds;
};

struct CompiledMoveEvent
{
    uint32_t eventId = 0;
    uint32_t tick = 0;
    std::string type;
    std::string payload;
    std::vector<uint32_t> tagIds;
};

struct CompiledMove
{
    uint32_t moveId = 0;
    std::string logicalId;
    uint32_t durationTicks = 0;
    InputTrigger input;
    MoveResources resources;
    InternTable internTable;
    std::vector<CompiledPhase> phases;
    std::vector<CompiledMovementTrack> movementTracks;
    std::vector<CompiledHitboxTrack> hitboxTracks;
    std::vector<CompiledHurtboxOverride> hurtboxOverrides;
    std::vector<CompiledCancelWindow> cancels;
    std::vector<CompiledMoveEvent> events;
};

struct MoveCompileResult
{
    ValidationResult validation;
    CompiledMove move;

    [[nodiscard]] bool ok() const { return validation.ok(); }
};

MoveAsset moveAssetFromJson(const nlohmann::json &document, std::filesystem::path sourcePath = {});
MoveAsset loadMoveAsset(const std::filesystem::path &path);
nlohmann::json toCanonicalJson(const MoveAsset &asset);
std::string toCanonicalJsonString(const MoveAsset &asset);
ValidationResult validateMoveAsset(const MoveAsset &asset);
MoveCompileResult compileMoveAsset(const MoveAsset &asset);
} // namespace vac::content
