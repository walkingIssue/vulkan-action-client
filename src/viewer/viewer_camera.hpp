#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vac::viewer
{
inline constexpr float kDefaultCameraDistanceWorldUnits = 42.0f;
inline constexpr float kMaxCameraDistanceWorldUnits = kDefaultCameraDistanceWorldUnits * 3.0f;
inline constexpr float kMinCameraDistanceWorldUnits = 0.0f;
inline constexpr float kCameraScrollZoomStepWorldUnits = 6.0f;

struct CameraView
{
    glm::vec3 eye{0.0f};
    glm::vec3 target{0.0f};
    float worldRadius = 32.0f;
};

struct CameraFrameInput
{
    bool worldBoundsValid = false;
    glm::vec3 worldBoundsMin{0.0f};
    glm::vec3 worldBoundsMax{0.0f};
    glm::vec3 anchorTranslation{0.0f};
    float yawDegrees = 180.0f;
    float pitchDegrees = 24.0f;
    float distanceWorldUnits = kDefaultCameraDistanceWorldUnits;
    float sceneTimeSeconds = 0.0f;
    bool orbitCamera = false;
};

CameraView buildCameraView(const CameraFrameInput &input);
glm::mat4 viewProjectionMatrix(const CameraView &camera, uint32_t width, uint32_t height);
} // namespace vac::viewer
