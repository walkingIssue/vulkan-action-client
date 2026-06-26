#include "viewer/viewer_camera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace vac::viewer
{
namespace
{
constexpr float kCameraFirstPersonBlendDistanceWorldUnits = 10.0f;
}

CameraView buildCameraView(const CameraFrameInput &input)
{
    CameraView camera;
    camera.worldRadius = input.worldBoundsValid
        ? std::max({input.worldBoundsMax.x - input.worldBoundsMin.x,
                    input.worldBoundsMax.y - input.worldBoundsMin.y,
                    input.worldBoundsMax.z - input.worldBoundsMin.z,
                    18.0f}) * 0.5f
        : 32.0f;

    const float yaw = glm::radians(input.yawDegrees);
    const float pitch = glm::radians(input.pitchDegrees);
    const glm::vec3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
    const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
    const glm::vec3 anchor = input.anchorTranslation + glm::vec3{0.0f, 6.5f, 0.0f};
    const float shoulderBlend = std::clamp(input.distanceWorldUnits / kCameraFirstPersonBlendDistanceWorldUnits,
                                           0.0f,
                                           1.0f);
    glm::vec3 cameraOffset = -forward * (std::cos(pitch) * input.distanceWorldUnits) +
                             right * (5.0f * shoulderBlend) +
                             glm::vec3{0.0f, std::sin(pitch) * input.distanceWorldUnits, 0.0f};

    if (input.orbitCamera) {
        const float orbitRadians = input.sceneTimeSeconds * 0.22f;
        const glm::mat4 orbit = glm::rotate(glm::mat4{1.0f}, orbitRadians, glm::vec3{0.0f, 1.0f, 0.0f});
        cameraOffset = glm::vec3{orbit * glm::vec4{cameraOffset, 0.0f}};
    }

    camera.eye = anchor + cameraOffset;
    camera.target = anchor + forward * 10.0f + glm::vec3{0.0f, 2.0f * shoulderBlend, 0.0f};
    return camera;
}

glm::mat4 viewProjectionMatrix(const CameraView &camera, uint32_t width, uint32_t height)
{
    glm::mat4 view = glm::lookAt(camera.eye, camera.target, glm::vec3{0.0f, 1.0f, 0.0f});
    glm::mat4 projection = glm::perspective(glm::radians(50.0f),
                                            static_cast<float>(width) / static_cast<float>(height),
                                            0.1f,
                                            std::max(100.0f, camera.worldRadius * 6.0f));
    projection[1][1] *= -1.0f;
    return projection * view;
}
} // namespace vac::viewer
