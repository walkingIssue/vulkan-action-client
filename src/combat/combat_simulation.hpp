#pragma once

#include <glm/glm.hpp>

#include "scene/scene_runtime.hpp"

namespace vac::combat
{
inline constexpr float kFixedTickSeconds = 1.0f / 60.0f;
inline constexpr int kMaxCatchUpTicksPerFrame = 5;

struct MovementTuning
{
    float playerRunSpeedWorldUnitsPerSecond = 37.7f;
    float playerSprintSpeedScale = 2.3f;
    float backpedalSpeedScale = 1.0f / 3.0f;
    float moveTargetArrivalRadiusWorldUnits = 1.0f;
    float sparringMoveSpeedWorldUnitsPerSecond = 50.0f;
    float turnSpeedDegreesPerSecond = 240.0f;
    float arenaEdgeInsetWorldUnits = 5.0f;
};

inline constexpr MovementTuning kDefaultMovementTuning{};

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
    float edgeInset = kDefaultMovementTuning.arenaEdgeInsetWorldUnits;
};

struct ActorState
{
    Transform previousTransform;
    Transform currentTransform;
    float moveSpeedWorldUnitsPerSecond = kDefaultMovementTuning.playerRunSpeedWorldUnitsPerSecond;
};

void beginTick(ActorState &actor);
bool applyCharacterLocomotion(ActorState &actor,
                              LocalMoveIntent intent,
                              ControlFrame idleControlFrame,
                              float deltaSeconds,
                              ArenaLimits arena,
                              const MovementTuning &tuning = kDefaultMovementTuning);
bool applyFramedStrafeLocomotion(ActorState &actor,
                                 LocalMoveIntent intent,
                                 ControlFrame movementFrame,
                                 bool lockFacingToMovementFrame,
                                 float deltaSeconds,
                                 ArenaLimits arena,
                                 const MovementTuning &tuning = kDefaultMovementTuning);
bool applyMoveToWorldTarget(ActorState &actor,
                            glm::vec2 target,
                            float deltaSeconds,
                            ArenaLimits arena,
                            const MovementTuning &tuning = kDefaultMovementTuning);
Transform interpolate(const ActorState &actor, float alpha);
} // namespace vac::combat
