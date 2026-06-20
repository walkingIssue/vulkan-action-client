#include "combat/combat_collision.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

#include <glm/geometric.hpp>

namespace vac::combat
{
namespace
{
glm::vec3 clampPoint(glm::vec3 point, glm::vec3 min, glm::vec3 max) { return glm::min(glm::max(point, min), max); }

float distanceSquaredPointSegment(glm::vec3 point, glm::vec3 start, glm::vec3 end)
{
    const glm::vec3 segment = end - start;
    const float lengthSquared = glm::dot(segment, segment);
    if (lengthSquared <= 0.000001f) {
        return glm::dot(point - start, point - start);
    }

    const float t = std::clamp(glm::dot(point - start, segment) / lengthSquared, 0.0f, 1.0f);
    const glm::vec3 closest = start + segment * t;
    return glm::dot(point - closest, point - closest);
}

float boundingRadius(const ResolvedVolume &volume)
{
    switch (volume.kind) {
    case CombatVolumeKind::sphere:
        return volume.radius;
    case CombatVolumeKind::capsule:
        return volume.radius + volume.halfHeight;
    case CombatVolumeKind::box:
        return glm::length(volume.halfExtents);
    }
    return volume.radius;
}

bool sphereSphere(const ResolvedVolume &left, const ResolvedVolume &right)
{
    const float radius = left.radius + right.radius;
    return glm::dot(left.center - right.center, left.center - right.center) <= radius * radius;
}

bool sphereBox(const ResolvedVolume &sphere, const ResolvedVolume &box)
{
    const glm::vec3 closest = clampPoint(sphere.center, box.center - box.halfExtents, box.center + box.halfExtents);
    return glm::dot(sphere.center - closest, sphere.center - closest) <= sphere.radius * sphere.radius;
}

bool boxBox(const ResolvedVolume &left, const ResolvedVolume &right)
{
    const glm::vec3 leftMin = left.center - left.halfExtents;
    const glm::vec3 leftMax = left.center + left.halfExtents;
    const glm::vec3 rightMin = right.center - right.halfExtents;
    const glm::vec3 rightMax = right.center + right.halfExtents;
    return leftMin.x <= rightMax.x && leftMax.x >= rightMin.x && leftMin.y <= rightMax.y && leftMax.y >= rightMin.y &&
           leftMin.z <= rightMax.z && leftMax.z >= rightMin.z;
}

ResolvedVolume capsuleAsSphere(const ResolvedVolume &capsule)
{
    ResolvedVolume sphere = capsule;
    sphere.kind = CombatVolumeKind::sphere;
    sphere.radius = boundingRadius(capsule);
    return sphere;
}

bool hasAnyTag(const std::vector<uint32_t> &tags, const std::vector<uint32_t> &blocked)
{
    for (uint32_t tag : tags) {
        if (std::find(blocked.begin(), blocked.end(), tag) != blocked.end()) {
            return true;
        }
    }
    return false;
}
} // namespace

bool isActiveOnTick(content::CompiledRange range, uint32_t tick) { return tick >= range.begin && tick < range.end; }

ResolvedVolume resolveVolume(const CombatVolume &volume, glm::vec3 rootTranslation,
                             const std::vector<SocketTransform> &sockets)
{
    glm::vec3 origin = rootTranslation;
    if (!volume.binding.socketName.empty()) {
        const auto it = std::find_if(sockets.begin(), sockets.end(), [&volume](const SocketTransform &socket) {
            return socket.name == volume.binding.socketName;
        });
        if (it != sockets.end()) {
            origin = it->translation;
        }
    }

    return {
        volume.kind, origin + volume.binding.localOffset, volume.halfExtents, volume.radius, volume.halfHeight,
    };
}

bool overlaps(const ResolvedVolume &left, const ResolvedVolume &right)
{
    if (left.kind == CombatVolumeKind::capsule) {
        return overlaps(capsuleAsSphere(left), right);
    }
    if (right.kind == CombatVolumeKind::capsule) {
        return overlaps(left, capsuleAsSphere(right));
    }
    if (left.kind == CombatVolumeKind::sphere && right.kind == CombatVolumeKind::sphere) {
        return sphereSphere(left, right);
    }
    if (left.kind == CombatVolumeKind::sphere && right.kind == CombatVolumeKind::box) {
        return sphereBox(left, right);
    }
    if (left.kind == CombatVolumeKind::box && right.kind == CombatVolumeKind::sphere) {
        return sphereBox(right, left);
    }
    return boxBox(left, right);
}

bool sweptIntersects(const ResolvedVolume &previous, const ResolvedVolume &current, const ResolvedVolume &target)
{
    if (overlaps(current, target)) {
        return true;
    }

    const float radius = boundingRadius(previous) + boundingRadius(target);
    return distanceSquaredPointSegment(target.center, previous.center, current.center) <= radius * radius;
}

std::vector<CombatHitCandidate> collectHitCandidates(const CombatHitQuery &query)
{
    std::vector<CombatHitCandidate> candidates;
    if (!isActiveOnTick(query.activeRange, query.tick)) {
        return candidates;
    }

    uint32_t subIndex = 0;
    for (const HurtVolume &target : query.targets) {
        if (hasAnyTag(target.stateTags, query.blockedByTargetTags)) {
            continue;
        }

        const bool hit = query.swept ? sweptIntersects(query.previousHitVolume, query.currentHitVolume, target.volume)
                                     : overlaps(query.currentHitVolume, target.volume);
        if (!hit) {
            continue;
        }

        candidates.push_back({
            query.attackerId,
            query.moveInstanceSequence,
            query.hitboxTrackId,
            target.victimId,
            subIndex++,
        });
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const CombatHitCandidate &left, const CombatHitCandidate &right) {
                  return std::tie(left.attackerId, left.moveInstanceSequence, left.hitboxTrackId, left.victimId,
                                  left.contactSubIndex) < std::tie(right.attackerId, right.moveInstanceSequence,
                                                                   right.hitboxTrackId, right.victimId,
                                                                   right.contactSubIndex);
              });
    return candidates;
}

std::vector<CombatHitCandidate> filterOncePerTarget(const std::vector<CombatHitCandidate> &candidates,
                                                    std::vector<uint32_t> &hitRegistry)
{
    std::vector<CombatHitCandidate> filtered;
    for (const CombatHitCandidate &candidate : candidates) {
        if (std::find(hitRegistry.begin(), hitRegistry.end(), candidate.victimId) != hitRegistry.end()) {
            continue;
        }
        hitRegistry.push_back(candidate.victimId);
        filtered.push_back(candidate);
    }
    return filtered;
}
} // namespace vac::combat
