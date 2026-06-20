#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "combat/combat_collision.hpp"

namespace
{
int g_failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

vac::combat::ResolvedVolume sphere(glm::vec3 center, float radius)
{
    return {
        vac::combat::CombatVolumeKind::sphere, center, {radius, radius, radius}, radius, radius,
    };
}

vac::combat::ResolvedVolume box(glm::vec3 center, glm::vec3 halfExtents)
{
    return {
        vac::combat::CombatVolumeKind::box, center, halfExtents, 0.0f, 0.0f,
    };
}

vac::combat::ResolvedVolume capsule(glm::vec3 center, float radius, float halfHeight)
{
    return {
        vac::combat::CombatVolumeKind::capsule, center, {radius, halfHeight, radius}, radius, halfHeight,
    };
}

vac::combat::CombatHitQuery baseQuery()
{
    return {
        9, {9, 13}, 1, 77, 12, sphere({0.0f, 0.0f, 0.0f}, 0.25f), sphere({0.0f, 0.0f, 0.0f}, 0.25f), false, {}, {},
    };
}

void activeWindowUsesHalfOpenTicks()
{
    const vac::content::CompiledRange range{9, 13};
    expect(!vac::combat::isActiveOnTick(range, 8), "hitbox inactive before begin tick");
    expect(vac::combat::isActiveOnTick(range, 9), "hitbox active on begin tick");
    expect(vac::combat::isActiveOnTick(range, 12), "hitbox active on final included tick");
    expect(!vac::combat::isActiveOnTick(range, 13), "hitbox inactive on end tick");
}

void rootAndProxySocketBindingResolveVolumes()
{
    vac::combat::CombatVolume rootVolume;
    rootVolume.kind = vac::combat::CombatVolumeKind::sphere;
    rootVolume.radius = 0.5f;
    rootVolume.binding.localOffset = {1.0f, 0.0f, 0.0f};
    const vac::combat::ResolvedVolume rootResolved = vac::combat::resolveVolume(rootVolume, {2.0f, 0.0f, 0.0f});
    expect(vac::combat::overlaps(rootResolved, sphere({3.25f, 0.0f, 0.0f}, 0.25f)),
           "root-bound volume resolves from root");

    vac::combat::CombatVolume socketVolume;
    socketVolume.kind = vac::combat::CombatVolumeKind::sphere;
    socketVolume.radius = 0.5f;
    socketVolume.binding.socketName = "weapon_tip";
    socketVolume.binding.localOffset = {0.0f, 0.0f, 0.5f};
    const vac::combat::ResolvedVolume socketResolved =
        vac::combat::resolveVolume(socketVolume, {0.0f, 0.0f, 0.0f}, {{"weapon_tip", {0.0f, 1.0f, 2.0f}}});
    expect(vac::combat::overlaps(socketResolved, sphere({0.0f, 1.0f, 2.75f}, 0.25f)),
           "proxy socket-bound volume resolves from socket");
}

void sweptWeaponMotionCatchesCrossedTarget()
{
    vac::combat::CombatHitQuery query = baseQuery();
    query.previousHitVolume = capsule({0.0f, 0.0f, -2.0f}, 0.1f, 0.25f);
    query.currentHitVolume = capsule({0.0f, 0.0f, 2.0f}, 0.1f, 0.25f);
    query.swept = true;
    query.targets = {{4, sphere({0.0f, 0.0f, 0.0f}, 0.2f), {}}};

    expect(!vac::combat::overlaps(query.currentHitVolume, query.targets.front().volume),
           "final pose alone misses crossed target");
    expect(vac::combat::collectHitCandidates(query).size() == 1, "swept query catches crossed target");
}

void stableOrderingIgnoresTargetInsertionOrder()
{
    vac::combat::CombatHitQuery first = baseQuery();
    first.currentHitVolume = box({0.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f});
    first.targets = {
        {3, sphere({0.0f, 0.0f, 0.0f}, 0.25f), {}},
        {2, sphere({1.0f, 0.0f, 0.0f}, 0.25f), {}},
        {5, sphere({2.0f, 0.0f, 0.0f}, 0.25f), {}},
    };

    vac::combat::CombatHitQuery second = first;
    std::reverse(second.targets.begin(), second.targets.end());

    const std::vector<vac::combat::CombatHitCandidate> firstHits = vac::combat::collectHitCandidates(first);
    const std::vector<vac::combat::CombatHitCandidate> secondHits = vac::combat::collectHitCandidates(second);
    expect(firstHits.size() == 3 && secondHits.size() == 3, "both insertion orders hit three targets");
    expect(firstHits.at(0).victimId == 2 && firstHits.at(1).victimId == 3 && firstHits.at(2).victimId == 5,
           "first order sorts victims");
    expect(secondHits.at(0).victimId == 2 && secondHits.at(1).victimId == 3 && secondHits.at(2).victimId == 5,
           "second order sorts victims identically");
}

void oncePerTargetRegistrySuppressesDuplicates()
{
    std::vector<vac::combat::CombatHitCandidate> candidates = {
        {1, 10, 2, 4, 0},
        {1, 10, 2, 4, 1},
        {1, 10, 2, 5, 2},
    };
    std::vector<uint32_t> registry;
    const std::vector<vac::combat::CombatHitCandidate> first = vac::combat::filterOncePerTarget(candidates, registry);
    const std::vector<vac::combat::CombatHitCandidate> second = vac::combat::filterOncePerTarget(candidates, registry);

    expect(first.size() == 2, "once-per-target keeps first hit per victim");
    expect(first.at(0).victimId == 4 && first.at(1).victimId == 5, "once-per-target preserves victim order");
    expect(second.empty(), "once-per-target suppresses already registered victims");
}

std::vector<uint32_t> dodgeTagsAtTick(uint32_t tick)
{
    constexpr uint32_t strikeInvulnerableTag = 7;
    if (tick >= 3 && tick < 11) {
        return {strikeInvulnerableTag};
    }
    return {};
}

void dodgeInvulnerabilityBoundary()
{
    constexpr uint32_t strikeInvulnerableTag = 7;
    for (const uint32_t tick : {2u, 3u, 10u, 11u}) {
        vac::combat::CombatHitQuery query = baseQuery();
        query.tick = tick;
        query.activeRange = {0, 20};
        query.blockedByTargetTags = {strikeInvulnerableTag};
        query.targets = {{2, sphere({0.0f, 0.0f, 0.0f}, 0.25f), dodgeTagsAtTick(tick)}};
        const bool hit = !vac::combat::collectHitCandidates(query).empty();
        if (tick == 2 || tick == 11) {
            expect(hit, "strike hit applies outside dodge invulnerability window");
        } else {
            expect(!hit, "strike hit is blocked inside dodge invulnerability window");
        }
    }
}
} // namespace

int main()
{
    try {
        activeWindowUsesHalfOpenTicks();
        rootAndProxySocketBindingResolveVolumes();
        sweptWeaponMotionCatchesCrossedTarget();
        stableOrderingIgnoresTargetInsertionOrder();
        oncePerTargetRegistrySuppressesDuplicates();
        dodgeInvulnerabilityBoundary();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
