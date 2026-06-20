#include "animation/proxy_animation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <glm/trigonometric.hpp>

namespace vac::animation
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

json vec3ToJson(glm::vec3 value) { return json::array({value.x, value.y, value.z}); }

bool finiteVec3(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

void addError(content::ValidationResult &result, const ProxyAnimationAsset &asset, std::string code,
              std::string component, std::string fieldPath, std::string message)
{
    result.diagnostics.push_back({
        "error",
        std::move(code),
        asset.sourcePath.string(),
        asset.logicalId,
        std::move(component),
        std::move(fieldPath),
        std::move(message),
    });
}

bool orderedAndInRange(const std::vector<RootKeyframe> &keys, uint32_t durationTicks)
{
    uint32_t previous = 0;
    bool first = true;
    for (const RootKeyframe &key : keys) {
        if (key.tick > durationTicks || (!first && key.tick <= previous) || !finiteVec3(key.translation) ||
            !std::isfinite(key.yawDegrees)) {
            return false;
        }
        previous = key.tick;
        first = false;
    }
    return true;
}

bool orderedAndInRange(const std::vector<SocketKeyframe> &keys, uint32_t durationTicks)
{
    uint32_t previous = 0;
    bool first = true;
    for (const SocketKeyframe &key : keys) {
        if (key.tick > durationTicks || (!first && key.tick <= previous) || !finiteVec3(key.translation) ||
            !finiteVec3(key.rotationDegrees)) {
            return false;
        }
        previous = key.tick;
        first = false;
    }
    return true;
}

RootKeyframe sampleRoot(const std::vector<RootKeyframe> &keys, float tick)
{
    if (keys.empty()) {
        return {};
    }
    if (tick <= static_cast<float>(keys.front().tick)) {
        return keys.front();
    }
    if (tick >= static_cast<float>(keys.back().tick)) {
        return keys.back();
    }

    for (size_t i = 1; i < keys.size(); ++i) {
        if (tick <= static_cast<float>(keys[i].tick)) {
            const RootKeyframe &left = keys[i - 1];
            const RootKeyframe &right = keys[i];
            const float t = (tick - static_cast<float>(left.tick)) / static_cast<float>(right.tick - left.tick);
            return {
                static_cast<uint32_t>(std::lround(tick)),
                glm::mix(left.translation, right.translation, t),
                left.yawDegrees + (right.yawDegrees - left.yawDegrees) * t,
            };
        }
    }
    return keys.back();
}

SocketKeyframe sampleSocket(const std::vector<SocketKeyframe> &keys, float tick)
{
    if (keys.empty()) {
        return {};
    }
    if (tick <= static_cast<float>(keys.front().tick)) {
        return keys.front();
    }
    if (tick >= static_cast<float>(keys.back().tick)) {
        return keys.back();
    }

    for (size_t i = 1; i < keys.size(); ++i) {
        if (tick <= static_cast<float>(keys[i].tick)) {
            const SocketKeyframe &left = keys[i - 1];
            const SocketKeyframe &right = keys[i];
            const float t = (tick - static_cast<float>(left.tick)) / static_cast<float>(right.tick - left.tick);
            return {
                static_cast<uint32_t>(std::lround(tick)),
                glm::mix(left.translation, right.translation, t),
                glm::mix(left.rotationDegrees, right.rotationDegrees, t),
            };
        }
    }
    return keys.back();
}

glm::vec3 rotateYaw(glm::vec3 value, float yawDegrees)
{
    const float yaw = glm::radians(yawDegrees);
    const float s = std::sin(yaw);
    const float c = std::cos(yaw);
    return {
        value.x * c + value.z * s,
        value.y,
        -value.x * s + value.z * c,
    };
}
} // namespace

ProxyAnimationAsset proxyAnimationFromJson(const nlohmann::json &document, std::filesystem::path sourcePath)
{
    ProxyAnimationAsset asset;
    asset.sourcePath = std::move(sourcePath);
    asset.schemaVersion = document.value("schemaVersion", 0u);
    asset.assetGuid = document.value("assetGuid", "");
    asset.logicalId = document.value("logicalId", "");
    asset.durationTicks = document.value("durationTicks", 0u);

    for (const json &item : document.value("rootMotion", json::array())) {
        asset.rootMotion.push_back({
            item.value("tick", 0u),
            readVec3(item.value("translation", json::array()), {0.0f, 0.0f, 0.0f}),
            item.value("yawDegrees", 0.0f),
        });
    }

    for (const json &socketItem : document.value("sockets", json::array())) {
        ProxySocketTrack track;
        track.name = socketItem.value("name", "");
        for (const json &key : socketItem.value("keyframes", json::array())) {
            track.keyframes.push_back({
                key.value("tick", 0u),
                readVec3(key.value("translation", json::array()), {0.0f, 0.0f, 0.0f}),
                readVec3(key.value("rotationDegrees", json::array()), {0.0f, 0.0f, 0.0f}),
            });
        }
        asset.sockets.push_back(std::move(track));
    }

    return asset;
}

ProxyAnimationAsset loadProxyAnimation(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open proxy animation '" + path.string() + "'");
    }

    json document;
    input >> document;
    return proxyAnimationFromJson(document, path);
}

nlohmann::json toCanonicalJson(const ProxyAnimationAsset &asset)
{
    std::vector<ProxySocketTrack> sockets = asset.sockets;
    std::sort(sockets.begin(), sockets.end(),
              [](const ProxySocketTrack &left, const ProxySocketTrack &right) { return left.name < right.name; });

    json root = json::array();
    for (const RootKeyframe &key : asset.rootMotion) {
        root.push_back(
            {{"tick", key.tick}, {"translation", vec3ToJson(key.translation)}, {"yawDegrees", key.yawDegrees}});
    }

    json socketArray = json::array();
    for (const ProxySocketTrack &track : sockets) {
        json keyframes = json::array();
        for (const SocketKeyframe &key : track.keyframes) {
            keyframes.push_back({{"rotationDegrees", vec3ToJson(key.rotationDegrees)},
                                 {"tick", key.tick},
                                 {"translation", vec3ToJson(key.translation)}});
        }
        socketArray.push_back({{"keyframes", std::move(keyframes)}, {"name", track.name}});
    }

    return {
        {"assetGuid", asset.assetGuid},  {"durationTicks", asset.durationTicks}, {"logicalId", asset.logicalId},
        {"rootMotion", std::move(root)}, {"schemaVersion", asset.schemaVersion}, {"sockets", std::move(socketArray)},
    };
}

std::string toCanonicalJsonString(const ProxyAnimationAsset &asset) { return toCanonicalJson(asset).dump(2) + "\n"; }

content::ValidationResult validateProxyAnimation(const ProxyAnimationAsset &asset)
{
    content::ValidationResult result;
    if (asset.schemaVersion != kProxyAnimationSchemaVersion) {
        addError(result, asset, "unsupported_schema_version", "ProxyAnimation", "schemaVersion",
                 "Unsupported proxy animation schema version");
    }
    if (asset.assetGuid.empty()) {
        addError(result, asset, "missing_asset_guid", "ProxyAnimation", "assetGuid",
                 "Proxy animation assetGuid is required");
    }
    if (asset.logicalId.empty()) {
        addError(result, asset, "missing_logical_id", "ProxyAnimation", "logicalId",
                 "Proxy animation logicalId is required");
    }
    if (asset.durationTicks == 0) {
        addError(result, asset, "invalid_duration", "ProxyAnimation", "durationTicks", "Duration must be positive");
    }
    if (asset.rootMotion.empty() || !orderedAndInRange(asset.rootMotion, asset.durationTicks)) {
        addError(result, asset, "invalid_root_track", "RootMotion", "rootMotion",
                 "Root keyframes must be finite, ordered by tick, and inside duration");
    }

    std::unordered_set<std::string> socketNames;
    for (const ProxySocketTrack &track : asset.sockets) {
        if (track.name.empty() || !socketNames.insert(track.name).second) {
            addError(result, asset, "invalid_socket_name", "SocketTrack", "sockets/name",
                     "Socket names must be non-empty and unique");
        }
        if (track.keyframes.empty() || !orderedAndInRange(track.keyframes, asset.durationTicks)) {
            addError(result, asset, "invalid_socket_track", "SocketTrack", "sockets/" + track.name,
                     "Socket keyframes must be finite, ordered by tick, and inside duration");
        }
    }

    for (std::string_view required : {"weapon_base", "weapon_tip", "chest"}) {
        if (!socketNames.contains(std::string{required})) {
            addError(result, asset, "missing_required_socket", "SocketTrack", "sockets/" + std::string{required},
                     "Proxy animation is missing a required sprint socket");
        }
    }

    return result;
}

ProxyPose sampleProxyPose(const ProxyAnimationAsset &asset, float tick)
{
    const RootKeyframe root = sampleRoot(asset.rootMotion, tick);
    ProxyPose pose;
    pose.tick = tick;
    pose.rootTranslation = root.translation;
    pose.rootYawDegrees = root.yawDegrees;

    std::vector<ProxySocketTrack> sockets = asset.sockets;
    std::sort(sockets.begin(), sockets.end(),
              [](const ProxySocketTrack &left, const ProxySocketTrack &right) { return left.name < right.name; });

    for (const ProxySocketTrack &track : sockets) {
        const SocketKeyframe socket = sampleSocket(track.keyframes, tick);
        SocketPose socketPose;
        socketPose.name = track.name;
        socketPose.localTranslation = socket.translation;
        socketPose.localRotationDegrees = socket.rotationDegrees;
        socketPose.worldTranslation = root.translation + rotateYaw(socket.translation, root.yawDegrees);
        socketPose.worldRotationDegrees = socket.rotationDegrees + glm::vec3{0.0f, root.yawDegrees, 0.0f};
        pose.sockets.push_back(socketPose);
    }

    return pose;
}

ProxyPose sampleProxyPoseAtTick(const ProxyAnimationAsset &asset, uint32_t tick)
{
    return sampleProxyPose(asset, static_cast<float>(tick));
}

glm::vec3 rootMotionDelta(const ProxyAnimationAsset &asset, float beginTick, float endTick)
{
    return sampleProxyPose(asset, endTick).rootTranslation - sampleProxyPose(asset, beginTick).rootTranslation;
}

const SocketPose *findSocket(const ProxyPose &pose, std::string_view socketName)
{
    const auto it = std::find_if(pose.sockets.begin(), pose.sockets.end(),
                                 [socketName](const SocketPose &socket) { return socket.name == socketName; });
    return it == pose.sockets.end() ? nullptr : &*it;
}
} // namespace vac::animation
