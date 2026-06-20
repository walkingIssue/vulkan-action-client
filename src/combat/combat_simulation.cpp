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

float lerp(float a, float b, float alpha)
{
    return a + (b - a) * alpha;
}
} // namespace

void beginTick(ActorState &actor)
{
    actor.previousTransform = actor.currentTransform;
}

MoveIntent toWorldMoveIntent(LocalMoveIntent intent, ControlFrame frame)
{
    const glm::vec2 axes = normalizedDirection(intent.axes);
    if (glm::dot(axes, axes) <= 0.0001f) {
        return {};
    }

    const float yawRadians = glm::radians(frame.yawDegrees);
    const glm::vec2 right{std::cos(yawRadians), -std::sin(yawRadians)};
    const glm::vec2 forward{std::sin(yawRadians), std::cos(yawRadians)};
    return {normalizedDirection(right * axes.x + forward * axes.y)};
}

bool applyMoveIntent(ActorState &actor,
                     MoveIntent intent,
                     float deltaSeconds,
                     ArenaLimits arena)
{
    const glm::vec2 direction = normalizedDirection(intent.worldDirection);
    if (glm::dot(direction, direction) <= 0.0001f) {
        return false;
    }

    Transform &transform = actor.currentTransform;
    transform.translation.x += direction.x * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
    transform.translation.z += direction.y * actor.moveSpeedWorldUnitsPerSecond * deltaSeconds;
    transform.rotationDegrees.y = glm::degrees(std::atan2(direction.x, direction.y));

    const glm::vec2 allowed = glm::max(arena.halfExtents - glm::vec2{arena.edgeInset}, glm::vec2{0.0f});
    transform.translation.x = std::clamp(transform.translation.x, -allowed.x, allowed.x);
    transform.translation.z = std::clamp(transform.translation.z, -allowed.y, allowed.y);
    return true;
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
