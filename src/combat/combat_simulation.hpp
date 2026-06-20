#pragma once

#include <glm/glm.hpp>

#include "scene/scene_runtime.hpp"

namespace vac::combat
{
inline constexpr float kFixedTickSeconds = 1.0f / 60.0f;
inline constexpr int kMaxCatchUpTicksPerFrame = 5;
inline constexpr float kPlayerMoveSpeedWorldUnitsPerSecond = 58.0f;
inline constexpr float kBackpedalSpeedScale = 1.0f / 3.0f;
inline constexpr float kSparringMoveSpeedWorldUnitsPerSecond = 50.0f;
inline constexpr float kTurnSpeedDegreesPerSecond = 240.0f;
inline constexpr float kArenaEdgeInsetWorldUnits = 5.0f;

struct LocalMoveIntent
{
    glm::vec2 axes{0.0f};
};

struct ControlFrame
{
    float yawDegrees = 0.0f;
};

struct ArenaLimits
{
    glm::vec2 halfExtents{200.0f};
    float edgeInset = kArenaEdgeInsetWorldUnits;
};

struct ActorState
{
    Transform previousTransform;
    Transform currentTransform;
    float moveSpeedWorldUnitsPerSecond = kPlayerMoveSpeedWorldUnitsPerSecond;
};

void beginTick(ActorState &actor);
bool applyCharacterLocomotion(ActorState &actor,
                              LocalMoveIntent intent,
                              ControlFrame idleControlFrame,
                              float deltaSeconds,
                              ArenaLimits arena);
bool applyFramedStrafeLocomotion(ActorState &actor,
                                 LocalMoveIntent intent,
                                 ControlFrame movementFrame,
                                 bool lockFacingToMovementFrame,
                                 float deltaSeconds,
                                 ArenaLimits arena);
Transform interpolate(const ActorState &actor, float alpha);
} // namespace vac::combat
