#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "content/authoring_scene.hpp"

namespace vac::animation
{
inline constexpr uint32_t kProxyAnimationSchemaVersion = 1;

struct RootKeyframe
{
    uint32_t tick = 0;
    glm::vec3 translation{0.0f};
    float yawDegrees = 0.0f;
};

struct SocketKeyframe
{
    uint32_t tick = 0;
    glm::vec3 translation{0.0f};
    glm::vec3 rotationDegrees{0.0f};
};

struct ProxySocketTrack
{
    std::string name;
    std::vector<SocketKeyframe> keyframes;
};

struct ProxyAnimationAsset
{
    std::filesystem::path sourcePath;
    uint32_t schemaVersion = kProxyAnimationSchemaVersion;
    std::string assetGuid;
    std::string logicalId;
    uint32_t durationTicks = 0;
    std::vector<RootKeyframe> rootMotion;
    std::vector<ProxySocketTrack> sockets;
};

struct SocketPose
{
    std::string name;
    glm::vec3 localTranslation{0.0f};
    glm::vec3 localRotationDegrees{0.0f};
    glm::vec3 worldTranslation{0.0f};
    glm::vec3 worldRotationDegrees{0.0f};
};

struct ProxyPose
{
    float tick = 0.0f;
    glm::vec3 rootTranslation{0.0f};
    float rootYawDegrees = 0.0f;
    std::vector<SocketPose> sockets;
};

ProxyAnimationAsset proxyAnimationFromJson(const nlohmann::json &document, std::filesystem::path sourcePath = {});
ProxyAnimationAsset loadProxyAnimation(const std::filesystem::path &path);
nlohmann::json toCanonicalJson(const ProxyAnimationAsset &asset);
std::string toCanonicalJsonString(const ProxyAnimationAsset &asset);
content::ValidationResult validateProxyAnimation(const ProxyAnimationAsset &asset);
ProxyPose sampleProxyPose(const ProxyAnimationAsset &asset, float tick);
ProxyPose sampleProxyPoseAtTick(const ProxyAnimationAsset &asset, uint32_t tick);
glm::vec3 rootMotionDelta(const ProxyAnimationAsset &asset, float beginTick, float endTick);
const SocketPose *findSocket(const ProxyPose &pose, std::string_view socketName);
} // namespace vac::animation
