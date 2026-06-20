#include "scene/scene_runtime.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

namespace vac
{
namespace
{
using json = nlohmann::json;

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

glm::vec2 readVec2(const json &value, glm::vec2 fallback)
{
    if (!value.is_array() || value.size() != 2) {
        return fallback;
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
    };
}

Transform readTransform(const json &value)
{
    Transform transform;
    if (!value.is_object()) {
        return transform;
    }

    transform.translation = readVec3(value.value("translation", json::array()), transform.translation);
    transform.rotationDegrees = readVec3(value.value("rotation_degrees", json::array()), transform.rotationDegrees);
    transform.scale = readVec3(value.value("scale", json::array()), transform.scale);
    return transform;
}

std::filesystem::path resolveProjectPath(const std::filesystem::path &path,
                                         const std::filesystem::path &projectRoot)
{
    if (path.is_absolute()) {
        return path;
    }

    return projectRoot / path;
}

void includePoint(Bounds &bounds, const glm::vec3 &point)
{
    if (!bounds.valid) {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }

    bounds.min = glm::min(bounds.min, point);
    bounds.max = glm::max(bounds.max, point);
}

void includeBounds(Bounds &target, const Bounds &source)
{
    if (!source.valid) {
        return;
    }

    includePoint(target, source.min);
    includePoint(target, source.max);
}

glm::mat4 transformMatrix(const Transform &transform)
{
    glm::mat4 matrix{1.0f};
    matrix = glm::translate(matrix, transform.translation);
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.y), glm::vec3{0.0f, 1.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.x), glm::vec3{1.0f, 0.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.z), glm::vec3{0.0f, 0.0f, 1.0f});
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

Bounds transformBoundsInternal(const Bounds &bounds, const Transform &transform)
{
    if (!bounds.valid) {
        return {};
    }

    const glm::mat4 matrix = transformMatrix(transform);
    const std::array<glm::vec3, 8> corners = {
        glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z},
    };

    Bounds transformed;
    for (const glm::vec3 &corner : corners) {
        const glm::vec4 world = matrix * glm::vec4{corner, 1.0f};
        includePoint(transformed, glm::vec3{world});
    }
    return transformed;
}

Bounds floorBounds(const glm::vec2 &size)
{
    const glm::vec2 half = size * 0.5f;
    Bounds bounds;
    includePoint(bounds, {-half.x, -0.01f, -half.y});
    includePoint(bounds, {half.x, 0.01f, half.y});
    return bounds;
}

ModelStats loadModel(const std::string &id, const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(fmt::format("Model '{}' does not exist at '{}'", id, path.string()));
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        path.string(),
        aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_SortByPType);

    if (!scene) {
        throw std::runtime_error(fmt::format("Assimp failed to load '{}': {}", path.string(), importer.GetErrorString()));
    }

    ModelStats stats;
    stats.id = id;
    stats.sourcePath = path;
    stats.rootNodeName = scene->mRootNode ? scene->mRootNode->mName.C_Str() : "";
    stats.meshCount = scene->mNumMeshes;
    stats.materialCount = scene->mNumMaterials;
    stats.animationCount = scene->mNumAnimations;

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh *mesh = scene->mMeshes[meshIndex];
        stats.vertexCount += mesh->mNumVertices;

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            if (mesh->mFaces[faceIndex].mNumIndices == 3) {
                ++stats.triangleCount;
            }
        }

        for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            const aiVector3D &vertex = mesh->mVertices[vertexIndex];
            includePoint(stats.localBounds, {vertex.x, vertex.y, vertex.z});
        }
    }

    return stats;
}
} // namespace

std::filesystem::path defaultProjectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

SceneRuntime loadScene(const std::filesystem::path &scenePath,
                       const std::filesystem::path &projectRoot)
{
    std::ifstream input(scenePath);
    if (!input) {
        throw std::runtime_error(fmt::format("Could not open scene file '{}'", scenePath.string()));
    }

    json document;
    input >> document;

    SceneRuntime runtime;
    runtime.name = document.value("name", scenePath.stem().string());

    for (const json &model : document.value("models", json::array())) {
        const std::string id = model.at("id").get<std::string>();
        const std::filesystem::path modelPath = resolveProjectPath(model.at("path").get<std::string>(), projectRoot);
        runtime.models.emplace(id, loadModel(id, modelPath));
    }

    for (const json &item : document.value("procedural", json::array())) {
        ProceduralInstance instance;
        instance.id = item.at("id").get<std::string>();
        instance.name = item.value("name", instance.id);
        instance.type = item.value("type", "unknown");
        instance.size = readVec2(item.value("size", json::array()), instance.size);
        instance.transform = readTransform(item.value("transform", json::object()));

        if (instance.type == "floor") {
            instance.worldBounds = transformBoundsInternal(floorBounds(instance.size), instance.transform);
        }

        includeBounds(runtime.worldBounds, instance.worldBounds);
        runtime.procedural.push_back(instance);
    }

    for (const json &entity : document.value("entities", json::array())) {
        SceneInstance instance;
        instance.id = entity.at("id").get<std::string>();
        instance.name = entity.value("name", instance.id);
        instance.modelId = entity.at("model").get<std::string>();
        instance.transform = readTransform(entity.value("transform", json::object()));

        const auto modelIt = runtime.models.find(instance.modelId);
        if (modelIt == runtime.models.end()) {
            throw std::runtime_error(fmt::format("Entity '{}' references unknown model '{}'", instance.id, instance.modelId));
        }

        instance.worldBounds = transformBoundsInternal(modelIt->second.localBounds, instance.transform);
        includeBounds(runtime.worldBounds, instance.worldBounds);
        runtime.instances.push_back(instance);
    }

    return runtime;
}

Bounds transformBounds(const Bounds &bounds, const Transform &transform)
{
    return transformBoundsInternal(bounds, transform);
}

void refreshSceneBounds(SceneRuntime &scene)
{
    scene.worldBounds = {};

    for (ProceduralInstance &instance : scene.procedural) {
        if (instance.type == "floor") {
            instance.worldBounds = transformBoundsInternal(floorBounds(instance.size), instance.transform);
        }
        includeBounds(scene.worldBounds, instance.worldBounds);
    }

    for (SceneInstance &instance : scene.instances) {
        const auto modelIt = scene.models.find(instance.modelId);
        if (modelIt == scene.models.end()) {
            throw std::runtime_error(fmt::format("Entity '{}' references unknown model '{}'", instance.id, instance.modelId));
        }

        instance.worldBounds = transformBoundsInternal(modelIt->second.localBounds, instance.transform);
        includeBounds(scene.worldBounds, instance.worldBounds);
    }
}

std::string formatBounds(const Bounds &bounds)
{
    if (!bounds.valid) {
        return "<empty>";
    }

    return fmt::format("min=({:.3f}, {:.3f}, {:.3f}) max=({:.3f}, {:.3f}, {:.3f})",
                       bounds.min.x,
                       bounds.min.y,
                       bounds.min.z,
                       bounds.max.x,
                       bounds.max.y,
                       bounds.max.z);
}
} // namespace vac
