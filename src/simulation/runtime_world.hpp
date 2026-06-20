#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "combat/combat_simulation.hpp"
#include "content/authoring_scene.hpp"

namespace vac::simulation
{
inline constexpr uint32_t kSimulationTickRate = 60;
inline constexpr double kFixedTickSeconds = 1.0 / static_cast<double>(kSimulationTickRate);

struct RuntimeActorId
{
    uint32_t value = 0;
};

struct InputFrame
{
    uint32_t tick = 0;
    RuntimeActorId actorId;
    glm::vec2 moveAxes{0.0f};
    float movementYawDegrees = 0.0f;
    bool cameraSteering = false;
    bool sprint = false;
    std::optional<glm::vec2> moveTarget;
};

struct RuntimeActor
{
    RuntimeActorId id;
    std::string spawnId;
    std::string team;
    combat::ActorState state;
    bool hasMoveTarget = false;
    glm::vec2 moveTarget{0.0f};
    bool movedLastTick = false;
    bool sprintingLastTick = false;
    bool cameraSteeringLastTick = false;
};

struct RuntimeWorld
{
    uint32_t tick = 0;
    content::WorldBounds worldBounds;
    combat::ArenaLimits arena;
    combat::MovementTuning tuning = combat::kDefaultMovementTuning;
    std::vector<RuntimeActor> actors;
};

struct TickResult
{
    uint32_t tick = 0;
    uint64_t stateHash = 0;
};

RuntimeWorld importRuntimeWorld(const content::RuntimeWorld &contentWorld,
                                const combat::MovementTuning &tuning = combat::kDefaultMovementTuning);
RuntimeActor *findActor(RuntimeWorld &world, RuntimeActorId actorId);
const RuntimeActor *findActor(const RuntimeWorld &world, RuntimeActorId actorId);
RuntimeActorId actorIdForSpawn(const RuntimeWorld &world, std::string_view spawnId);
TickResult advanceFixedTick(RuntimeWorld &world, const std::vector<InputFrame> &inputFrames);
void runFixedTicks(RuntimeWorld &world, const std::vector<InputFrame> &inputFrames, uint32_t tickCount);
RuntimeWorld runWithRenderCadence(RuntimeWorld world, const std::vector<InputFrame> &inputFrames,
                                  const std::vector<double> &frameDeltasSeconds);
uint64_t stateHash(const RuntimeWorld &world);
Transform presentationTransform(const RuntimeWorld &world, RuntimeActorId actorId, float alpha);
} // namespace vac::simulation
