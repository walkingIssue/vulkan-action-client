#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace vac
{
struct Bounds
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    bool valid = false;
};

struct Transform
{
    glm::vec3 translation{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
};

struct ModelStats
{
    std::string id;
    std::filesystem::path sourcePath;
    std::string rootNodeName;
    uint32_t meshCount = 0;
    uint32_t materialCount = 0;
    uint32_t animationCount = 0;
    uint64_t vertexCount = 0;
    uint64_t triangleCount = 0;
    Bounds localBounds;
};

struct SceneInstance
{
    std::string id;
    std::string name;
    std::string modelId;
    Transform transform;
    Bounds worldBounds;
};

struct ProceduralInstance
{
    std::string id;
    std::string name;
    std::string type;
    glm::vec2 size{1.0f};
    Transform transform;
    Bounds worldBounds;
};

struct SceneRuntime
{
    std::string name;
    std::unordered_map<std::string, ModelStats> models;
    std::vector<SceneInstance> instances;
    std::vector<ProceduralInstance> procedural;
    Bounds worldBounds;
};

std::filesystem::path defaultProjectRoot();
SceneRuntime loadScene(const std::filesystem::path &scenePath,
                       const std::filesystem::path &projectRoot);
Bounds transformBounds(const Bounds &bounds, const Transform &transform);
void refreshSceneBounds(SceneRuntime &scene);
std::string formatBounds(const Bounds &bounds);
} // namespace vac
