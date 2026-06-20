#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "content/move_asset.hpp"

namespace vac::combat
{
enum class CombatVolumeKind {
    sphere,
    capsule,
    box,
};

struct VolumeBinding
{
    std::string socketName;
    glm::vec3 localOffset{0.0f};
};

struct CombatVolume
{
    CombatVolumeKind kind = CombatVolumeKind::sphere;
    VolumeBinding binding;
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    float halfHeight = 0.5f;
};

struct SocketTransform
{
    std::string name;
    glm::vec3 translation{0.0f};
};

struct ResolvedVolume
{
    CombatVolumeKind kind = CombatVolumeKind::sphere;
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    float halfHeight = 0.5f;
};

struct HurtVolume
{
    uint32_t victimId = 0;
    ResolvedVolume volume;
    std::vector<uint32_t> stateTags;
};

struct CombatHitCandidate
{
    uint32_t attackerId = 0;
    uint32_t moveInstanceSequence = 0;
    uint32_t hitboxTrackId = 0;
    uint32_t victimId = 0;
    uint32_t contactSubIndex = 0;
};

struct CombatHitQuery
{
    uint32_t tick = 0;
    content::CompiledRange activeRange;
    uint32_t attackerId = 0;
    uint32_t moveInstanceSequence = 0;
    uint32_t hitboxTrackId = 0;
    ResolvedVolume previousHitVolume;
    ResolvedVolume currentHitVolume;
    bool swept = false;
    std::vector<uint32_t> blockedByTargetTags;
    std::vector<HurtVolume> targets;
};

bool isActiveOnTick(content::CompiledRange range, uint32_t tick);
ResolvedVolume resolveVolume(const CombatVolume &volume, glm::vec3 rootTranslation,
                             const std::vector<SocketTransform> &sockets = {});
bool overlaps(const ResolvedVolume &left, const ResolvedVolume &right);
bool sweptIntersects(const ResolvedVolume &previous, const ResolvedVolume &current, const ResolvedVolume &target);
std::vector<CombatHitCandidate> collectHitCandidates(const CombatHitQuery &query);
std::vector<CombatHitCandidate> filterOncePerTarget(const std::vector<CombatHitCandidate> &candidates,
                                                    std::vector<uint32_t> &hitRegistry);
} // namespace vac::combat
