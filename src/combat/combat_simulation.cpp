#include "combat/combat_simulation.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace vac::combat
{
namespace
{
glm::vec2 normalizedDirection(glm::vec2 direction)
{
    const float lengthSquared = glm::dot(direction, direction);
    if (lengthSquared > 1.0f) {
        return direction / std::sqrt(lengthSquared);
    }
    return direction;
}

glm::vec2 forwardFromYaw(float yawDegrees)
{
    const float yawRadians = glm::radians(yawDegrees);
    return {std::sin(yawRadians), std::cos(yawRadians)};
}

glm::vec2 rightFromYaw(float yawDegrees)
{
    const float yawRadians = glm::radians(yawDegrees);
    return {std::cos(yawRadians), -std::sin(yawRadians)};
}

void clampToArena(Transform &transform, ArenaLimits arena)
{
    const glm::vec2 allowed = glm::max(arena.halfExtents - glm::vec2{arena.edgeInset}, glm::vec2{0.0f});
    transform.translation.x = std::clamp(transform.translation.x, -allowed.x, allowed.x);
    transform.translation.z = std::clamp(transform.translation.z, -allowed.y, allowed.y);
}
} // namespace

void beginTick(ActorState &actor)
{
    actor.previousTransform = actor.currentTransform;
}

bool applyCharacterLocomotion(ActorState &actor,
                              LocalMoveIntent intent,
                              ControlFrame idleControlFrame,
                              float deltaSeconds,
                              ArenaLimits arena)
{
    const glm::vec2 axes = normalizedDirection(intent.axes);
    const float turnAxis = axes.x;
    const float forwardAxis = axes.y;
    const bool wantsForwardMotion = std::abs(forwardAxis) > 0.0001f;
    const bool wantsSideStart = !wantsForwardMotion && std::abs(turnAxis) > 0.0001f;

    if (!wantsForwardMotion && !wantsSideStart) {
        return false;
    }

    Transform &transform = actor.currentTransform;

    if (wantsForwardMotion) {
        transform.rotationDegrees.y += turnAxis * kTurnSpeedDegreesPerSecond * deltaSeconds;

        const glm::vec2 direction = forwardFromYaw(transform.rotationDegrees.y) * forwardAxis;
        transform.translation.x += direction.x * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
        transform.translation.z += direction.y * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
    } else {
        transform.rotationDegrees.y = idleControlFrame.yawDegrees + turnAxis * 90.0f;

        const glm::vec2 direction = forwardFromYaw(transform.rotationDegrees.y);
        transform.translation.x += direction.x * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
        transform.translation.z += direction.y * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
    }

    clampToArena(transform, arena);
    return true;
}

bool applyFramedStrafeLocomotion(ActorState &actor,
                                 LocalMoveIntent intent,
                                 ControlFrame movementFrame,
                                 bool lockFacingToMovementFrame,
                                 float deltaSeconds,
                                 ArenaLimits arena)
{
    Transform &transform = actor.currentTransform;
    const glm::vec2 axes = normalizedDirection(intent.axes);
    const bool wantsBackpedal = axes.y < -0.0001f;
    bool rotated = false;

    if (lockFacingToMovementFrame) {
        rotated = std::abs(transform.rotationDegrees.y - movementFrame.yawDegrees) > 0.001f;
        transform.rotationDegrees.y = movementFrame.yawDegrees;
    }

    if (glm::dot(axes, axes) <= 0.0001f) {
        return rotated;
    }

    const glm::vec2 direction = normalizedDirection(
        rightFromYaw(movementFrame.yawDegrees) * axes.x +
        forwardFromYaw(movementFrame.yawDegrees) * axes.y);
    const float speedScale = wantsBackpedal ? kBackpedalSpeedScale : 1.0f;

    transform.translation.x += direction.x * actor.moveSpeedWorldUnitsPerSecond * speedScale * deltaSeconds;
    transform.translation.z += direction.y * actor.moveSpeedWorldUnitsPerSecond * speedScale * deltaSeconds;
    clampToArena(transform, arena);
    return true;
}

bool applyMoveToWorldTarget(ActorState &actor,
                            glm::vec2 target,
                            float deltaSeconds,
                            ArenaLimits arena,
                            float arrivalRadius)
{
    Transform &transform = actor.currentTransform;
    const glm::vec2 current{transform.translation.x, transform.translation.z};
    const glm::vec2 toTarget = target - current;
    const float distance = std::sqrt(glm::dot(toTarget, toTarget));
    if (distance <= arrivalRadius) {
        return false;
    }

    const glm::vec2 direction = toTarget / distance;
    const float yawDegrees = glm::degrees(std::atan2(direction.x, direction.y));
    const bool rotated = std::abs(transform.rotationDegrees.y - yawDegrees) > 0.001f;
    transform.rotationDegrees.y = yawDegrees;

    const float step = std::min(actor.moveSpeedWorldUnitsPerSecond * deltaSeconds, distance);
    transform.translation.x += direction.x * step;
    transform.translation.z += direction.y * step;
    clampToArena(transform, arena);
    return rotated || step > 0.0f;
}

Transform interpolate(const ActorState &actor, float alpha)
{
    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);

    Transform transform;
    transform.translation = glm::mix(actor.previousTransform.translation,
                                     actor.currentTransform.translation,
                                     clampedAlpha);
    transform.rotationDegrees = glm::mix(actor.previousTransform.rotationDegrees,
                                         actor.currentTransform.rotationDegrees,
                                         clampedAlpha);
    transform.scale = glm::mix(actor.previousTransform.scale,
                               actor.currentTransform.scale,
                               clampedAlpha);
    return transform;
}
} // namespace vac::combat
