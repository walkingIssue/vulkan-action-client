#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace vac::content
{
inline constexpr uint32_t kAuthoringSceneSchemaVersion = 1;

struct AuthoredObjectId
{
    std::string value;
};

struct RuntimeEntityId
{
    uint32_t value = 0;
};

enum class PrimitiveKind {
    unknown,
    plane,
    box,
    sphere,
    capsule,
    cylinder,
};

enum class ColliderKind {
    unknown,
    box,
    sphere,
    capsule,
};

struct AuthoringTransform
{
    glm::vec3 translation{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
};

struct PrimitiveShape
{
    PrimitiveKind kind = PrimitiveKind::unknown;
    glm::vec3 size{1.0f};
};

struct MaterialOverride
{
    glm::vec4 baseColor{0.7f, 0.7f, 0.7f, 1.0f};
};

struct Collider
{
    ColliderKind kind = ColliderKind::unknown;
    glm::vec3 size{1.0f};
    std::string mode = "static";
    std::string layer = "world";
    std::vector<std::string> mask;
};

struct AuthoringObject
{
    AuthoredObjectId id;
    std::string name;
    std::optional<AuthoredObjectId> parentId;
    AuthoringTransform transform;
    PrimitiveShape primitive;
    MaterialOverride material;
    std::optional<Collider> collider;
};

struct SpawnPoint
{
    std::string id;
    std::string team;
    glm::vec3 translation{0.0f};
    float yawDegrees = 0.0f;
    bool enabled = true;
    std::vector<std::string> tags;
};

struct WorldBounds
{
    glm::vec3 min{-10.0f};
    glm::vec3 max{10.0f};
    float killHeight = -10.0f;
};

struct AuthoringScene
{
    std::filesystem::path sourcePath;
    uint32_t schemaVersion = kAuthoringSceneSchemaVersion;
    std::string assetGuid;
    std::string logicalId;
    float unitsPerMeter = 1.0f;
    std::string coordinateSystem = "RH_Y_UP_NEG_Z_FORWARD";
    WorldBounds worldBounds;
    std::vector<AuthoringObject> objects;
    std::vector<SpawnPoint> spawnPoints;
};

struct ContentDiagnostic
{
    std::string severity = "error";
    std::string code;
    std::string file;
    std::string objectId;
    std::string component;
    std::string fieldPath;
    std::string message;
};

struct ValidationResult
{
    std::vector<ContentDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

struct Bounds3
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

struct RuntimeEntity
{
    RuntimeEntityId id;
    AuthoredObjectId authoredId;
    AuthoringTransform transform;
    PrimitiveKind primitiveKind = PrimitiveKind::unknown;
    Bounds3 localBounds;
    glm::vec4 baseColor{1.0f};
};

struct RuntimeCollider
{
    RuntimeEntityId entityId;
    ColliderKind kind = ColliderKind::unknown;
    Bounds3 localBounds;
    std::string mode;
    std::string layer;
    std::vector<std::string> mask;
};

struct RuntimeSpawnPoint
{
    std::string id;
    std::string team;
    glm::vec3 translation{0.0f};
    float yawDegrees = 0.0f;
    std::vector<std::string> tags;
};

struct RuntimeWorld
{
    WorldBounds worldBounds;
    std::vector<RuntimeEntity> entities;
    std::vector<RuntimeCollider> colliders;
    std::vector<RuntimeSpawnPoint> spawnPoints;
};

struct CompileResult
{
    ValidationResult validation;
    RuntimeWorld world;

    [[nodiscard]] bool ok() const { return validation.ok(); }
};

AuthoringScene authoringSceneFromJson(const nlohmann::json &document, std::filesystem::path sourcePath = {});
AuthoringScene loadAuthoringScene(const std::filesystem::path &path);
nlohmann::json toCanonicalJson(const AuthoringScene &scene);
std::string toCanonicalJsonString(const AuthoringScene &scene);
void saveAuthoringScene(const AuthoringScene &scene, const std::filesystem::path &path);
ValidationResult validateAuthoringScene(const AuthoringScene &scene);
CompileResult compileRuntimeWorld(const AuthoringScene &scene);

std::string toString(PrimitiveKind kind);
std::string toString(ColliderKind kind);
} // namespace vac::content
