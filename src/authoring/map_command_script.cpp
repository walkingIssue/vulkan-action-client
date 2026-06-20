#include "authoring/map_command_script.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <glm/glm.hpp>

#include "simulation/runtime_world.hpp"

namespace vac::authoring
{
namespace
{
using json = nlohmann::json;

glm::vec3 readVec3(const json &value, glm::vec3 fallback)
{
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

glm::vec4 readVec4(const json &value, glm::vec4 fallback)
{
    if (!value.is_array() || value.size() != 4) {
        return fallback;
    }
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>()};
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

std::string slugFromName(std::string_view name)
{
    std::string slug;
    bool lastWasUnderscore = false;
    for (const char ch : name) {
        const auto unsignedCh = static_cast<unsigned char>(ch);
        if (std::isalnum(unsignedCh)) {
            slug.push_back(static_cast<char>(std::tolower(unsignedCh)));
            lastWasUnderscore = false;
            continue;
        }
        if (!lastWasUnderscore && !slug.empty()) {
            slug.push_back('_');
            lastWasUnderscore = true;
        }
    }
    while (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }
    return slug.empty() ? "object" : slug;
}

content::PrimitiveKind primitiveKindFromString(const std::string &value)
{
    if (value == "plane") {
        return content::PrimitiveKind::plane;
    }
    if (value == "box") {
        return content::PrimitiveKind::box;
    }
    if (value == "sphere") {
        return content::PrimitiveKind::sphere;
    }
    if (value == "capsule") {
        return content::PrimitiveKind::capsule;
    }
    if (value == "cylinder") {
        return content::PrimitiveKind::cylinder;
    }
    return content::PrimitiveKind::unknown;
}

content::ColliderKind colliderKindFromString(const std::string &value)
{
    if (value == "box") {
        return content::ColliderKind::box;
    }
    if (value == "sphere") {
        return content::ColliderKind::sphere;
    }
    if (value == "capsule") {
        return content::ColliderKind::capsule;
    }
    return content::ColliderKind::unknown;
}

content::ColliderKind colliderKindForPrimitive(content::PrimitiveKind kind)
{
    switch (kind) {
    case content::PrimitiveKind::sphere:
        return content::ColliderKind::sphere;
    case content::PrimitiveKind::capsule:
        return content::ColliderKind::capsule;
    case content::PrimitiveKind::box:
    case content::PrimitiveKind::plane:
    case content::PrimitiveKind::cylinder:
    case content::PrimitiveKind::unknown:
        return content::ColliderKind::box;
    }
    return content::ColliderKind::box;
}

std::filesystem::path resolvePath(const std::filesystem::path &path, const std::filesystem::path &workingDirectory)
{
    if (path.is_absolute() || workingDirectory.empty()) {
        return path;
    }
    return workingDirectory / path;
}

void addDiagnostic(MapCommandResult &result,
                   uint32_t commandIndex,
                   std::string code,
                   std::string fieldPath,
                   std::string message,
                   std::string objectId = {},
                   std::string component = "Command")
{
    result.status = "error";
    result.diagnostics.push_back({
        "error",
        std::move(code),
        commandIndex,
        {},
        std::move(objectId),
        std::move(component),
        std::move(fieldPath),
        std::move(message),
    });
}

void addValidationDiagnostics(MapCommandResult &result,
                              const content::ValidationResult &validation,
                              uint32_t commandIndex)
{
    for (const content::ContentDiagnostic &diagnostic : validation.diagnostics) {
        result.status = "error";
        result.diagnostics.push_back({
            diagnostic.severity,
            diagnostic.code,
            commandIndex,
            diagnostic.file,
            diagnostic.objectId,
            diagnostic.component,
            diagnostic.fieldPath,
            diagnostic.message,
        });
    }
}

content::AuthoringObject *findObject(content::AuthoringScene &scene, std::string_view objectRef)
{
    auto it = std::find_if(scene.objects.begin(), scene.objects.end(), [objectRef](const content::AuthoringObject &object) {
        return object.id.value == objectRef || object.name == objectRef;
    });
    return it == scene.objects.end() ? nullptr : &*it;
}

content::AuthoringScene basicTemplate(const json &command)
{
    content::AuthoringScene scene;
    scene.sourcePath = std::filesystem::path{};
    scene.schemaVersion = content::kAuthoringSceneSchemaVersion;
    scene.assetGuid = command.value("assetGuid", "map-guid-command-script");
    scene.logicalId = command.value("logicalId", "map.command_script");
    scene.unitsPerMeter = 1.0f;
    scene.coordinateSystem = "RH_Y_UP_NEG_Z_FORWARD";
    scene.worldBounds = {{-10.0f, -1.0f, -10.0f}, {10.0f, 5.0f, 10.0f}, -6.0f};
    return scene;
}

void applyTransformFields(content::AuthoringTransform &transform, const json &command)
{
    transform.translation =
        readVec3(command.value("translation", command.value("position", json::array())), transform.translation);
    transform.rotationDegrees =
        readVec3(command.value("rotationDegrees", command.value("rotation", json::array())), transform.rotationDegrees);
    transform.scale = readVec3(command.value("scale", json::array()), transform.scale);
}

bool compileScene(MapCommandResult &result, uint32_t commandIndex, content::CompileResult &compiled)
{
    compiled = content::compileRuntimeWorld(result.scene);
    result.entityCount = static_cast<uint32_t>(compiled.world.entities.size());
    result.colliderCount = static_cast<uint32_t>(compiled.world.colliders.size());
    result.spawnPointCount = static_cast<uint32_t>(compiled.world.spawnPoints.size());
    addValidationDiagnostics(result, compiled.validation, commandIndex);
    return compiled.ok();
}

bool playTicks(MapCommandResult &result, uint32_t commandIndex, uint32_t ticks)
{
    content::CompileResult compiled;
    if (!compileScene(result, commandIndex, compiled)) {
        return false;
    }

    simulation::RuntimeWorld world = simulation::importRuntimeWorld(compiled.world);
    simulation::runFixedTicks(world, {}, ticks);
    result.ticksPlayed += ticks;
    result.stateHash = simulation::stateHash(world);
    return true;
}

json diagnosticToJson(const MapCommandDiagnostic &diagnostic)
{
    return {
        {"severity", diagnostic.severity},
        {"code", diagnostic.code},
        {"commandIndex", diagnostic.commandIndex},
        {"file", diagnostic.file},
        {"objectId", diagnostic.objectId},
        {"component", diagnostic.component},
        {"fieldPath", diagnostic.fieldPath},
        {"message", diagnostic.message},
    };
}

void executeCommand(MapCommandResult &result,
                    const json &command,
                    uint32_t commandIndex,
                    const MapCommandRunOptions &options,
                    content::CompileResult &lastCompile)
{
    if (!command.is_object()) {
        addDiagnostic(result, commandIndex, "invalid_command", "commands", "Command entry must be an object");
        return;
    }

    const std::string name = command.value("command", "");
    if (name.empty()) {
        addDiagnostic(result, commandIndex, "missing_command", "command", "Command name is required");
        return;
    }

    if (name == "newFromTemplate") {
        const std::string templateId = command.value("template", "scene.template.basic_render");
        if (templateId != "scene.template.basic_render" && templateId != "map.template.basic_primitive_arena") {
            addDiagnostic(result, commandIndex, "unknown_template", "template", "Template is not supported");
            return;
        }
        result.scene = basicTemplate(command);
        lastCompile = {};
        return;
    }

    if (name == "createPrimitive") {
        const std::string objectName = command.value("name", command.value("id", "Primitive"));
        std::string objectId = command.value("id", slugFromName(objectName));
        if (findObject(result.scene, objectId) != nullptr) {
            addDiagnostic(result, commandIndex, "duplicate_object", "id", "Object id already exists", objectId, "Object");
            return;
        }

        content::AuthoringObject object;
        object.id = {std::move(objectId)};
        object.name = objectName;
        object.primitive.kind = primitiveKindFromString(command.value("type", "box"));
        object.primitive.size = readVec3(command.value("size", json::array()), object.primitive.size);
        object.material.baseColor = readVec4(command.value("baseColor", json::array()), object.material.baseColor);
        applyTransformFields(object.transform, command);
        result.createdObjectIds.push_back(object.id.value);
        result.scene.objects.push_back(std::move(object));
        lastCompile = {};
        return;
    }

    const std::string objectRef = command.value("object", "");
    content::AuthoringObject *object = objectRef.empty() ? nullptr : findObject(result.scene, objectRef);

    if (name == "setTransform") {
        if (object == nullptr) {
            addDiagnostic(result, commandIndex, "missing_object", "object", "setTransform target was not found", objectRef,
                          "Transform");
            return;
        }
        applyTransformFields(object->transform, command);
        lastCompile = {};
        return;
    }

    if (name == "setBaseColor") {
        if (object == nullptr) {
            addDiagnostic(result, commandIndex, "missing_object", "object", "setBaseColor target was not found", objectRef,
                          "MaterialOverride");
            return;
        }
        object->material.baseColor = readVec4(command.value("value", json::array()), object->material.baseColor);
        lastCompile = {};
        return;
    }

    if (name == "addCollider") {
        if (object == nullptr) {
            addDiagnostic(result, commandIndex, "missing_object", "object", "addCollider target was not found", objectRef,
                          "Collider");
            return;
        }

        content::Collider collider;
        if (command.contains("type")) {
            collider.kind = colliderKindFromString(command.at("type").get<std::string>());
        } else {
            collider.kind = colliderKindForPrimitive(object->primitive.kind);
        }
        collider.size = readVec3(command.value("size", json::array()), object->primitive.size);
        collider.mode = command.value("mode", collider.mode);
        collider.layer = command.value("layer", collider.layer);
        collider.mask = readStringVector(command.value("mask", json::array()));
        object->collider = std::move(collider);
        lastCompile = {};
        return;
    }

    if (name == "addSpawnPoint") {
        content::SpawnPoint spawn;
        spawn.id = command.value("id", "");
        spawn.team = command.value("team", "");
        spawn.translation = readVec3(command.value("translation", command.value("position", json::array())),
                                     spawn.translation);
        spawn.yawDegrees = command.value("yawDegrees", 0.0f);
        spawn.enabled = command.value("enabled", true);
        spawn.tags = readStringVector(command.value("tags", json::array()));
        result.scene.spawnPoints.push_back(std::move(spawn));
        lastCompile = {};
        return;
    }

    if (name == "setBounds") {
        result.scene.worldBounds.min = readVec3(command.value("min", json::array()), result.scene.worldBounds.min);
        result.scene.worldBounds.max = readVec3(command.value("max", json::array()), result.scene.worldBounds.max);
        result.scene.worldBounds.killHeight = command.value("killHeight", result.scene.worldBounds.killHeight);
        lastCompile = {};
        return;
    }

    if (name == "saveAs" || name == "save") {
        const std::optional<std::filesystem::path> requestedPath =
            command.contains("path") ? std::optional<std::filesystem::path>{command.at("path").get<std::string>()}
                                     : result.savedPath;
        if (!requestedPath.has_value()) {
            addDiagnostic(result, commandIndex, "missing_save_path", "path", "save requires a path");
            return;
        }
        const std::filesystem::path savePath = resolvePath(*requestedPath, options.workingDirectory);
        if (!savePath.parent_path().empty()) {
            std::filesystem::create_directories(savePath.parent_path());
        }
        content::saveAuthoringScene(result.scene, savePath);
        result.savedPath = savePath;
        result.scene.sourcePath = savePath;
        return;
    }

    if (name == "reopen") {
        const std::optional<std::filesystem::path> requestedPath =
            command.contains("path") ? std::optional<std::filesystem::path>{command.at("path").get<std::string>()}
                                     : result.savedPath;
        if (!requestedPath.has_value()) {
            addDiagnostic(result, commandIndex, "missing_reopen_path", "path", "reopen requires a path");
            return;
        }
        const std::filesystem::path reopenPath = resolvePath(*requestedPath, options.workingDirectory);
        result.scene = content::loadAuthoringScene(reopenPath);
        result.savedPath = reopenPath;
        lastCompile = {};
        return;
    }

    if (name == "compile") {
        (void)compileScene(result, commandIndex, lastCompile);
        return;
    }

    if (name == "playTicks") {
        (void)playTicks(result, commandIndex, command.value("ticks", 0u));
        return;
    }

    if (name == "stop") {
        return;
    }

    addDiagnostic(result, commandIndex, "unknown_command", "command", "Command is not supported");
}
} // namespace

MapCommandResult runMapCommandDocument(const nlohmann::json &document, const MapCommandRunOptions &options)
{
    MapCommandResult result;
    const json commands = document.is_array() ? document : document.value("commands", json::array());
    if (!commands.is_array()) {
        addDiagnostic(result, 0, "invalid_script", "commands", "Command script must be an array or contain commands");
        result.message = "Command script failed";
        return result;
    }

    result.commandCount = static_cast<uint32_t>(commands.size());
    content::CompileResult lastCompile;
    for (size_t i = 0; i < commands.size(); ++i) {
        executeCommand(result, commands.at(i), static_cast<uint32_t>(i), options, lastCompile);
        if (!result.ok()) {
            result.message = "Command script failed";
            return result;
        }
    }

    if (options.defaultPlayTicks.has_value() && result.ticksPlayed == 0) {
        (void)playTicks(result, result.commandCount, *options.defaultPlayTicks);
    }

    if (result.ok()) {
        result.message = "Command script completed";
    } else if (result.message.empty()) {
        result.message = "Command script failed";
    }
    return result;
}

MapCommandResult runMapCommandScript(const std::filesystem::path &path, const MapCommandRunOptions &options)
{
    std::ifstream input{path};
    if (!input) {
        MapCommandResult result;
        addDiagnostic(result, 0, "script_open_failed", path.string(), "Could not open command script");
        result.message = "Command script failed";
        return result;
    }

    json document;
    input >> document;

    MapCommandRunOptions resolvedOptions = options;
    if (resolvedOptions.workingDirectory.empty()) {
        resolvedOptions.workingDirectory = std::filesystem::current_path();
    }
    return runMapCommandDocument(document, resolvedOptions);
}

nlohmann::json mapCommandResultToJson(const MapCommandResult &result, std::string host)
{
    json diagnostics = json::array();
    for (const MapCommandDiagnostic &diagnostic : result.diagnostics) {
        diagnostics.push_back(diagnosticToJson(diagnostic));
    }

    json document{
        {"host", std::move(host)},
        {"status", result.status},
        {"message", result.message},
        {"commandCount", result.commandCount},
        {"createdObjectIds", result.createdObjectIds},
        {"diagnostics", std::move(diagnostics)},
        {"entityCount", result.entityCount},
        {"colliderCount", result.colliderCount},
        {"spawnPointCount", result.spawnPointCount},
        {"ticks", result.ticksPlayed},
        {"stateHash", result.stateHash},
    };
    if (result.savedPath.has_value()) {
        document["savedPath"] = result.savedPath->string();
    }
    return document;
}
} // namespace vac::authoring
