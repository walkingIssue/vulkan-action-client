#include "render/scene_geometry.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>

namespace vac
{
namespace
{
struct MeshVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

struct MeshGeometry
{
    std::vector<MeshVertex> vertices;
};

MeshGeometry loadMeshGeometry(const ModelStats &model)
{
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        model.sourcePath.string(),
        aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_GenSmoothNormals |
            aiProcess_SortByPType);

    if (!scene) {
        throw std::runtime_error(fmt::format("Assimp failed to load render geometry '{}': {}",
                                             model.sourcePath.string(),
                                             importer.GetErrorString()));
    }

    MeshGeometry geometry;
    geometry.vertices.reserve(static_cast<size_t>(model.triangleCount * 3));

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh *mesh = scene->mMeshes[meshIndex];
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace &face = mesh->mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }

            for (uint32_t corner = 0; corner < 3; ++corner) {
                const uint32_t vertexIndex = face.mIndices[corner];
                const aiVector3D &position = mesh->mVertices[vertexIndex];
                const aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[vertexIndex] : aiVector3D{0.0f, 1.0f, 0.0f};
                geometry.vertices.push_back({
                    {position.x, position.y, position.z},
                    {normal.x, normal.y, normal.z},
                });
            }
        }
    }

    return geometry;
}

void appendLine(SceneDrawData &drawData, glm::vec3 a, glm::vec3 b, glm::vec3 color)
{
    drawData.lineVertices.push_back({a, {0.0f, 1.0f, 0.0f}, color});
    drawData.lineVertices.push_back({b, {0.0f, 1.0f, 0.0f}, color});
}

void appendBounds(SceneDrawData &drawData, const Bounds &bounds, glm::vec3 color)
{
    if (!bounds.valid) {
        return;
    }

    const std::array<glm::vec3, 8> p = {
        glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    };

    appendLine(drawData, p[0], p[1], color);
    appendLine(drawData, p[1], p[2], color);
    appendLine(drawData, p[2], p[3], color);
    appendLine(drawData, p[3], p[0], color);
    appendLine(drawData, p[4], p[5], color);
    appendLine(drawData, p[5], p[6], color);
    appendLine(drawData, p[6], p[7], color);
    appendLine(drawData, p[7], p[4], color);
    appendLine(drawData, p[0], p[4], color);
    appendLine(drawData, p[1], p[5], color);
    appendLine(drawData, p[2], p[6], color);
    appendLine(drawData, p[3], p[7], color);
}

void appendFloor(SceneDrawData &drawData, const ProceduralInstance &floor)
{
    const glm::vec2 half = floor.size * 0.5f;
    const glm::mat4 transform = makeTransformMatrix(floor.transform);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    const glm::vec3 normal = glm::normalize(normalMatrix * glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::vec3 color{0.20f, 0.22f, 0.24f};

    auto tx = [&](glm::vec3 p) {
        return glm::vec3{transform * glm::vec4{p, 1.0f}};
    };

    const glm::vec3 a = tx({-half.x, 0.0f, -half.y});
    const glm::vec3 b = tx({half.x, 0.0f, -half.y});
    const glm::vec3 c = tx({half.x, 0.0f, half.y});
    const glm::vec3 d = tx({-half.x, 0.0f, half.y});

    drawData.triangleVertices.push_back({a, normal, color});
    drawData.triangleVertices.push_back({b, normal, color});
    drawData.triangleVertices.push_back({c, normal, color});
    drawData.triangleVertices.push_back({a, normal, color});
    drawData.triangleVertices.push_back({c, normal, color});
    drawData.triangleVertices.push_back({d, normal, color});

    const glm::vec3 gridColor{0.36f, 0.39f, 0.42f};
    const int divisions = std::clamp(static_cast<int>(std::ceil(std::max(floor.size.x, floor.size.y))), 18, 48);
    for (int i = 0; i <= divisions; ++i) {
        const float t = -0.5f + static_cast<float>(i) / static_cast<float>(divisions);
        appendLine(drawData, tx({t * floor.size.x, 0.015f, -half.y}), tx({t * floor.size.x, 0.015f, half.y}), gridColor);
        appendLine(drawData, tx({-half.x, 0.015f, t * floor.size.y}), tx({half.x, 0.015f, t * floor.size.y}), gridColor);
    }
}
} // namespace

glm::vec3 instanceColor(size_t index)
{
    static constexpr std::array<glm::vec3, 6> colors = {
        glm::vec3{0.78f, 0.74f, 0.66f},
        glm::vec3{0.46f, 0.72f, 0.95f},
        glm::vec3{0.94f, 0.58f, 0.44f},
        glm::vec3{0.60f, 0.86f, 0.55f},
        glm::vec3{0.85f, 0.66f, 0.95f},
        glm::vec3{0.96f, 0.86f, 0.44f},
    };
    return colors[index % colors.size()];
}

glm::mat4 makeTransformMatrix(const Transform &transform)
{
    glm::mat4 matrix{1.0f};
    matrix = glm::translate(matrix, transform.translation);
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.y), glm::vec3{0.0f, 1.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.x), glm::vec3{1.0f, 0.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.z), glm::vec3{0.0f, 0.0f, 1.0f});
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

SceneRenderData buildSceneRenderData(const SceneRuntime &scene)
{
    SceneRenderData renderData;

    for (const auto &[modelId, model] : scene.models) {
        const MeshGeometry geometry = loadMeshGeometry(model);
        std::vector<SceneVertex> vertices;
        vertices.reserve(geometry.vertices.size());
        for (const MeshVertex &vertex : geometry.vertices) {
            vertices.push_back({vertex.position, vertex.normal, {1.0f, 1.0f, 1.0f}});
        }
        renderData.modelVerticesById.emplace(modelId, std::move(vertices));
    }

    SceneDrawData staticDrawData;
    for (const ProceduralInstance &procedural : scene.procedural) {
        if (procedural.type == "floor") {
            appendFloor(staticDrawData, procedural);
            appendBounds(staticDrawData, procedural.worldBounds, {0.45f, 0.52f, 0.58f});
        }
    }
    appendBounds(staticDrawData, scene.worldBounds, {0.40f, 0.95f, 0.72f});

    renderData.staticTriangleVertices = std::move(staticDrawData.triangleVertices);
    renderData.staticLineVertices = std::move(staticDrawData.lineVertices);
    return renderData;
}

SceneDrawData buildSceneLineData(const SceneRuntime &scene)
{
    SceneDrawData drawData;

    for (const SceneInstance &instance : scene.instances) {
        appendBounds(drawData, instance.worldBounds, {1.0f, 0.92f, 0.36f});
    }

    return drawData;
}

} // namespace vac
