#pragma once

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

glm::mat4 makeTransformMatrix(const Transform &transform);
SceneDrawData buildSceneDrawData(const SceneRuntime &scene);
} // namespace vac
