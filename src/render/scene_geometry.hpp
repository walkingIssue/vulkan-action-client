#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "scene/scene_runtime.hpp"

namespace vac
{
struct SceneVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f};
};

struct SceneDrawData
{
    std::vector<SceneVertex> triangleVertices;
    std::vector<SceneVertex> lineVertices;
};

struct SceneRenderData
{
    std::vector<SceneVertex> staticTriangleVertices;
    std::vector<SceneVertex> staticLineVertices;
    std::unordered_map<std::string, std::vector<SceneVertex>> modelVerticesById;
};

glm::mat4 makeTransformMatrix(const Transform &transform);
glm::vec3 instanceColor(size_t index);
SceneRenderData buildSceneRenderData(const SceneRuntime &scene);
SceneDrawData buildSceneLineData(const SceneRuntime &scene);
} // namespace vac
