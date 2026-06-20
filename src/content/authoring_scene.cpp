#include "content/authoring_scene.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vac::content
{
namespace
{
using json = nlohmann::json;

constexpr float kMinimumScale = 0.0001f;

PrimitiveKind primitiveKindFromString(const std::string &value)
{
    if (value == "plane") {
        return PrimitiveKind::plane;
    }
    if (value == "box") {
        return PrimitiveKind::box;
    }
    if (value == "sphere") {
        return PrimitiveKind::sphere;
    }
    if (value == "capsule") {
        return PrimitiveKind::capsule;
    }
    if (value == "cylinder") {
        return PrimitiveKind::cylinder;
    }
    return PrimitiveKind::unknown;
}

ColliderKind colliderKindFromString(const std::string &value)
{
    if (value == "box") {
        return ColliderKind::box;
    }
    if (value == "sphere") {
        return ColliderKind::sphere;
    }
    if (value == "capsule") {
        return ColliderKind::capsule;
    }
    return ColliderKind::unknown;
}

json vec3ToJson(glm::vec3 value) { return json::array({value.x, value.y, value.z}); }

json vec4ToJson(glm::vec4 value) { return json::array({value.x, value.y, value.z, value.w}); }

glm::vec3 readVec3(const json &value, glm::vec3 fallback)
{
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

glm::vec4 readVec4(const json &value, glm::vec4 fallback)
{
    if (!value.is_array() || value.size() != 4) {
        return fallback;
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
        value.at(3).get<float>(),
    };
}

std::vector<std::string> readStringVector(const json &value)
{
    std::vector<std::string> result;
    if (!value.is_array()) {
        return result;
    }

    result.reserve(value.size());
    for (const json &item : value) {
        result.push_back(item.get<std::string>());
    }
    return result;
}

json sortedStringArray(std::vector<std::string> values)
{
    std::sort(values.begin(), values.end());
    return values;
}

AuthoringTransform readTransform(const json &value)
{
    AuthoringTransform transform;
    if (!value.is_object()) {
        return transform;
    }

    transform.translation = readVec3(value.value("translation", json::array()), transform.translation);
    transform.rotationDegrees = readVec3(value.value("rotationDegrees", json::array()), transform.rotationDegrees);
    transform.scale = readVec3(value.value("scale", json::array()), transform.scale);
    return transform;
}

json transformToJson(const AuthoringTransform &transform)
{
    return {
        {"rotationDegrees", vec3ToJson(transform.rotationDegrees)},
        {"scale", vec3ToJson(transform.scale)},
        {"translation", vec3ToJson(transform.translation)},
    };
}

WorldBounds readWorldBounds(const json &value)
{
    WorldBounds bounds;
    if (!value.is_object()) {
        return bounds;
    }

    bounds.min = readVec3(value.value("min", json::array()), bounds.min);
    bounds.max = readVec3(value.value("max", json::array()), bounds.max);
    bounds.killHeight = value.value("killHeight", bounds.killHeight);
    return bounds;
}

json worldBoundsToJson(const WorldBounds &bounds)
{
    return {
        {"killHeight", bounds.killHeight},
        {"max", vec3ToJson(bounds.max)},
        {"min", vec3ToJson(bounds.min)},
    };
}

PrimitiveShape readPrimitive(const json &value)
{
    PrimitiveShape primitive;
    if (!value.is_object()) {
        return primitive;
    }

    primitive.kind = primitiveKindFromString(value.value("type", ""));
    primitive.size = readVec3(value.value("size", json::array()), primitive.size);
    return primitive;
}

json primitiveToJson(const PrimitiveShape &primitive)
{
    return {
        {"size", vec3ToJson(primitive.size)},
        {"type", toString(primitive.kind)},
    };
}

MaterialOverride readMaterial(const json &value)
{
    MaterialOverride material;
    if (!value.is_object()) {
        return material;
    }

    material.baseColor = readVec4(value.value("baseColor", json::array()), material.baseColor);
    return material;
}

json materialToJson(const MaterialOverride &material)
{
    return {
        {"baseColor", vec4ToJson(material.baseColor)},
    };
}

Collider readCollider(const json &value)
{
    Collider collider;
    if (!value.is_object()) {
        return collider;
    }

    collider.kind = colliderKindFromString(value.value("type", ""));
    collider.size = readVec3(value.value("size", json::array()), collider.size);
    collider.mode = value.value("mode", collider.mode);
    collider.layer = value.value("layer", collider.layer);
    collider.mask = readStringVector(value.value("mask", json::array()));
    return collider;
}

json colliderToJson(const Collider &collider)
{
    return {
        {"layer", collider.layer},           {"mask", sortedStringArray(collider.mask)}, {"mode", collider.mode},
        {"size", vec3ToJson(collider.size)}, {"type", toString(collider.kind)},
    };
}

bool finiteVec3(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

bool finiteVec4(glm::vec4 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
}

bool positiveVec3(glm::vec3 value) { return finiteVec3(value) && value.x > 0.0f && value.y > 0.0f && value.z > 0.0f; }

bool validScale(glm::vec3 value)
{
    return finiteVec3(value) && std::fabs(value.x) >= kMinimumScale && std::fabs(value.y) >= kMinimumScale &&
           std::fabs(value.z) >= kMinimumScale;
}

bool validColor(glm::vec4 value)
{
    return finiteVec4(value) && value.x >= 0.0f && value.x <= 1.0f && value.y >= 0.0f && value.y <= 1.0f &&
           value.z >= 0.0f && value.z <= 1.0f && value.w >= 0.0f && value.w <= 1.0f;
}

void addError(ValidationResult &result, const AuthoringScene &scene, std::string code, std::string objectId,
              std::string component, std::string fieldPath, std::string message)
{
    result.diagnostics.push_back({
        "error",
        std::move(code),
        scene.sourcePath.string(),
        std::move(objectId),
        std::move(component),
        std::move(fieldPath),
        std::move(message),
    });
}

bool worldBoundsValid(const WorldBounds &bounds)
{
    return finiteVec3(bounds.min) && finiteVec3(bounds.max) && std::isfinite(bounds.killHeight) &&
           bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y && bounds.min.z < bounds.max.z;
}

Bounds3 primitiveLocalBounds(const PrimitiveShape &primitive)
{
    switch (primitive.kind) {
    case PrimitiveKind::plane:
        return {{-primitive.size.x * 0.5f, 0.0f, -primitive.size.z * 0.5f},
                {primitive.size.x * 0.5f, 0.0f, primitive.size.z * 0.5f}};
    case PrimitiveKind::sphere:
        return {{-primitive.size.x, -primitive.size.x, -primitive.size.x},
                {primitive.size.x, primitive.size.x, primitive.size.x}};
    case PrimitiveKind::capsule:
    case PrimitiveKind::cylinder:
    case PrimitiveKind::box:
    case PrimitiveKind::unknown:
        return {{-primitive.size.x * 0.5f, -primitive.size.y * 0.5f, -primitive.size.z * 0.5f},
                {primitive.size.x * 0.5f, primitive.size.y * 0.5f, primitive.size.z * 0.5f}};
    }

    return {};
}

Bounds3 colliderLocalBounds(const Collider &collider)
{
    switch (collider.kind) {
    case ColliderKind::sphere:
        return {{-collider.size.x, -collider.size.x, -collider.size.x},
                {collider.size.x, collider.size.x, collider.size.x}};
    case ColliderKind::box:
    case ColliderKind::capsule:
    case ColliderKind::unknown:
        return {{-collider.size.x * 0.5f, -collider.size.y * 0.5f, -collider.size.z * 0.5f},
                {collider.size.x * 0.5f, collider.size.y * 0.5f, collider.size.z * 0.5f}};
    }

    return {};
}
} // namespace

std::string toString(PrimitiveKind kind)
{
    switch (kind) {
    case PrimitiveKind::plane:
        return "plane";
    case PrimitiveKind::box:
        return "box";
    case PrimitiveKind::sphere:
        return "sphere";
    case PrimitiveKind::capsule:
        return "capsule";
    case PrimitiveKind::cylinder:
        return "cylinder";
    case PrimitiveKind::unknown:
        return "unknown";
    }

    return "unknown";
}

std::string toString(ColliderKind kind)
{
    switch (kind) {
    case ColliderKind::box:
        return "box";
    case ColliderKind::sphere:
        return "sphere";
    case ColliderKind::capsule:
        return "capsule";
    case ColliderKind::unknown:
        return "unknown";
    }

    return "unknown";
}

AuthoringScene authoringSceneFromJson(const nlohmann::json &document, std::filesystem::path sourcePath)
{
    AuthoringScene scene;
    scene.sourcePath = std::move(sourcePath);
    scene.schemaVersion = document.value("schemaVersion", 0u);
    scene.assetGuid = document.value("assetGuid", "");
    scene.logicalId = document.value("logicalId", "");
    scene.unitsPerMeter = document.value("unitsPerMeter", 1.0f);
    scene.coordinateSystem = document.value("coordinateSystem", "RH_Y_UP_NEG_Z_FORWARD");
    scene.worldBounds = readWorldBounds(document.value("worldBounds", json::object()));

    for (const json &item : document.value("objects", json::array())) {
        AuthoringObject object;
        object.id = {item.value("id", "")};
        object.name = item.value("name", object.id.value);
        if (item.contains("parent") && !item.at("parent").is_null()) {
            object.parentId = AuthoredObjectId{item.at("parent").get<std::string>()};
        }
        object.transform = readTransform(item.value("transform", json::object()));
        object.primitive = readPrimitive(item.value("primitive", json::object()));
        object.material = readMaterial(item.value("material", json::object()));
        if (item.contains("collider") && !item.at("collider").is_null()) {
            object.collider = readCollider(item.at("collider"));
        }
        scene.objects.push_back(std::move(object));
    }

    for (const json &item : document.value("spawnPoints", json::array())) {
        SpawnPoint spawn;
        spawn.id = item.value("id", "");
        spawn.team = item.value("team", "");
        spawn.translation = readVec3(item.value("translation", json::array()), spawn.translation);
        spawn.yawDegrees = item.value("yawDegrees", 0.0f);
        spawn.enabled = item.value("enabled", true);
        spawn.tags = readStringVector(item.value("tags", json::array()));
        scene.spawnPoints.push_back(std::move(spawn));
    }

    return scene;
}

AuthoringScene loadAuthoringScene(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open authoring scene '" + path.string() + "'");
    }

    json document;
    input >> document;
    return authoringSceneFromJson(document, path);
}

nlohmann::json toCanonicalJson(const AuthoringScene &scene)
{
    std::vector<AuthoringObject> sortedObjects = scene.objects;
    std::sort(sortedObjects.begin(), sortedObjects.end(),
              [](const AuthoringObject &left, const AuthoringObject &right) { return left.id.value < right.id.value; });

    std::vector<SpawnPoint> sortedSpawns = scene.spawnPoints;
    std::sort(sortedSpawns.begin(), sortedSpawns.end(),
              [](const SpawnPoint &left, const SpawnPoint &right) { return left.id < right.id; });

    json objects = json::array();
    for (const AuthoringObject &object : sortedObjects) {
        json item{
            {"collider", object.collider ? colliderToJson(*object.collider) : json(nullptr)},
            {"id", object.id.value},
            {"material", materialToJson(object.material)},
            {"name", object.name},
            {"primitive", primitiveToJson(object.primitive)},
            {"transform", transformToJson(object.transform)},
        };
        if (object.parentId.has_value()) {
            item["parent"] = object.parentId->value;
        }
        objects.push_back(std::move(item));
    }

    json spawnPoints = json::array();
    for (const SpawnPoint &spawn : sortedSpawns) {
        spawnPoints.push_back({
            {"enabled", spawn.enabled},
            {"id", spawn.id},
            {"tags", sortedStringArray(spawn.tags)},
            {"team", spawn.team},
            {"translation", vec3ToJson(spawn.translation)},
            {"yawDegrees", spawn.yawDegrees},
        });
    }

    return {
        {"assetGuid", scene.assetGuid},         {"coordinateSystem", scene.coordinateSystem},
        {"logicalId", scene.logicalId},         {"objects", std::move(objects)},
        {"schemaVersion", scene.schemaVersion}, {"spawnPoints", std::move(spawnPoints)},
        {"unitsPerMeter", scene.unitsPerMeter}, {"worldBounds", worldBoundsToJson(scene.worldBounds)},
    };
}

std::string toCanonicalJsonString(const AuthoringScene &scene) { return toCanonicalJson(scene).dump(2) + "\n"; }

void saveAuthoringScene(const AuthoringScene &scene, const std::filesystem::path &path)
{
    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        throw std::runtime_error("Could not open authoring scene for write '" + path.string() + "'");
    }
    output << toCanonicalJsonString(scene);
}

ValidationResult validateAuthoringScene(const AuthoringScene &scene)
{
    ValidationResult result;

    if (scene.schemaVersion != kAuthoringSceneSchemaVersion) {
        addError(result, scene, "unsupported_schema_version", "", "Scene", "schemaVersion",
                 "Authoring scene schema version is not supported");
    }
    if (scene.assetGuid.empty()) {
        addError(result, scene, "missing_asset_guid", "", "Scene", "assetGuid",
                 "Authoring scene assetGuid is required");
    }
    if (scene.logicalId.empty()) {
        addError(result, scene, "missing_logical_id", "", "Scene", "logicalId",
                 "Authoring scene logicalId is required");
    }
    if (!std::isfinite(scene.unitsPerMeter) || scene.unitsPerMeter <= 0.0f) {
        addError(result, scene, "invalid_units_per_meter", "", "Scene", "unitsPerMeter",
                 "unitsPerMeter must be finite and positive");
    }
    if (!worldBoundsValid(scene.worldBounds)) {
        addError(result, scene, "invalid_world_bounds", "", "WorldBounds", "worldBounds",
                 "worldBounds min/max and killHeight must be finite, with min less than max");
    }

    std::unordered_set<std::string> objectIds;
    for (const AuthoringObject &object : scene.objects) {
        if (object.id.value.empty()) {
            addError(result, scene, "missing_object_id", "", "Object", "objects/id", "Object id is required");
            continue;
        }
        if (!objectIds.insert(object.id.value).second) {
            addError(result, scene, "duplicate_object_id", object.id.value, "Object", "objects/" + object.id.value,
                     "Object id is duplicated");
        }
    }

    for (const AuthoringObject &object : scene.objects) {
        const std::string objectPath = "objects/" + object.id.value;
        if (object.parentId.has_value() && !objectIds.contains(object.parentId->value)) {
            addError(result, scene, "missing_parent", object.id.value, "Transform", objectPath + "/parent",
                     "Object parent does not exist");
        }

        if (!finiteVec3(object.transform.translation) || !finiteVec3(object.transform.rotationDegrees) ||
            !validScale(object.transform.scale)) {
            addError(result, scene, "invalid_transform", object.id.value, "Transform", objectPath + "/transform",
                     "Transform translation/rotation must be finite and scale must be non-zero");
        }

        if (object.primitive.kind == PrimitiveKind::unknown || !positiveVec3(object.primitive.size)) {
            addError(result, scene, "invalid_primitive", object.id.value, "PrimitiveShape", objectPath + "/primitive",
                     "Primitive type must be known and size must be finite and positive");
        }

        if (!validColor(object.material.baseColor)) {
            addError(result, scene, "invalid_material", object.id.value, "MaterialOverride",
                     objectPath + "/material/baseColor", "Material baseColor values must be finite and in [0, 1]");
        }

        if (object.collider.has_value()) {
            const Collider &collider = *object.collider;
            if (collider.kind == ColliderKind::unknown || !positiveVec3(collider.size) || collider.mode.empty() ||
                collider.layer.empty()) {
                addError(result, scene, "invalid_collider", object.id.value, "Collider", objectPath + "/collider/size",
                         "Collider type, size, mode, and layer must be valid");
            }
        }
    }

    std::unordered_set<std::string> spawnIds;
    size_t enabledSpawnCount = 0;
    for (const SpawnPoint &spawn : scene.spawnPoints) {
        if (spawn.enabled) {
            ++enabledSpawnCount;
        }

        if (spawn.id.empty()) {
            addError(result, scene, "missing_spawn_id", "", "SpawnPoint", "spawnPoints/id",
                     "Spawn point id is required");
            continue;
        }
        if (!spawnIds.insert(spawn.id).second) {
            addError(result, scene, "duplicate_spawn_id", spawn.id, "SpawnPoint", "spawnPoints/" + spawn.id,
                     "Spawn point id is duplicated");
        }
        if (!finiteVec3(spawn.translation) || !std::isfinite(spawn.yawDegrees) || spawn.team.empty()) {
            addError(result, scene, "invalid_spawn_point", spawn.id, "SpawnPoint", "spawnPoints/" + spawn.id,
                     "Spawn point translation/yaw/team must be valid");
        }
    }

    if (enabledSpawnCount == 0) {
        addError(result, scene, "missing_spawn_points", "", "SpawnPoint", "spawnPoints",
                 "At least one enabled spawn point is required");
    }

    return result;
}

CompileResult compileRuntimeWorld(const AuthoringScene &scene)
{
    CompileResult result;
    result.validation = validateAuthoringScene(scene);
    if (!result.validation.ok()) {
        return result;
    }

    result.world.worldBounds = scene.worldBounds;

    std::vector<const AuthoringObject *> sortedObjects;
    sortedObjects.reserve(scene.objects.size());
    for (const AuthoringObject &object : scene.objects) {
        sortedObjects.push_back(&object);
    }
    std::sort(
        sortedObjects.begin(), sortedObjects.end(),
        [](const AuthoringObject *left, const AuthoringObject *right) { return left->id.value < right->id.value; });

    std::unordered_map<std::string, RuntimeEntityId> runtimeIdByAuthoredId;
    uint32_t nextRuntimeId = 1;
    for (const AuthoringObject *object : sortedObjects) {
        const RuntimeEntityId runtimeId{nextRuntimeId++};
        runtimeIdByAuthoredId[object->id.value] = runtimeId;
        result.world.entities.push_back({
            runtimeId,
            object->id,
            object->transform,
            object->primitive.kind,
            primitiveLocalBounds(object->primitive),
            object->material.baseColor,
        });
    }

    for (const AuthoringObject *object : sortedObjects) {
        if (!object->collider.has_value()) {
            continue;
        }

        const Collider &collider = *object->collider;
        result.world.colliders.push_back({
            runtimeIdByAuthoredId.at(object->id.value),
            collider.kind,
            colliderLocalBounds(collider),
            collider.mode,
            collider.layer,
            collider.mask,
        });
    }

    std::vector<SpawnPoint> sortedSpawns = scene.spawnPoints;
    std::sort(sortedSpawns.begin(), sortedSpawns.end(),
              [](const SpawnPoint &left, const SpawnPoint &right) { return left.id < right.id; });
    for (const SpawnPoint &spawn : sortedSpawns) {
        if (!spawn.enabled) {
            continue;
        }

        result.world.spawnPoints.push_back({
            spawn.id,
            spawn.team,
            spawn.translation,
            spawn.yawDegrees,
            spawn.tags,
        });
    }

    return result;
}
} // namespace vac::content
