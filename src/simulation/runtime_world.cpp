#include "simulation/runtime_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace vac::simulation
{
namespace
{
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

bool hasAxes(glm::vec2 axes) { return std::abs(axes.x) > 0.0001f || std::abs(axes.y) > 0.0001f; }

glm::vec2 arenaHalfExtents(const content::WorldBounds &bounds)
{
    return {
        std::max((bounds.max.x - bounds.min.x) * 0.5f, 0.0f),
        std::max((bounds.max.z - bounds.min.z) * 0.5f, 0.0f),
    };
}

int32_t quantizeFloat(float value)
{
    if (!std::isfinite(value)) {
        return 0;
    }

    const double scaled = static_cast<double>(value) * 1000.0;
    const double clamped = std::clamp(scaled, static_cast<double>(std::numeric_limits<int32_t>::min()),
                                      static_cast<double>(std::numeric_limits<int32_t>::max()));
    return static_cast<int32_t>(std::llround(clamped));
}

void appendByte(uint64_t &hash, uint8_t value)
{
    hash ^= value;
    hash *= kFnvPrime;
}

void appendUint32(uint64_t &hash, uint32_t value)
{
    for (int byteIndex = 0; byteIndex < 4; ++byteIndex) {
        appendByte(hash, static_cast<uint8_t>((value >> (byteIndex * 8)) & 0xffu));
    }
}

void appendInt32(uint64_t &hash, int32_t value) { appendUint32(hash, static_cast<uint32_t>(value)); }

void appendBool(uint64_t &hash, bool value) { appendByte(hash, value ? 1u : 0u); }

void applyInput(RuntimeWorld &world, RuntimeActor &actor, const InputFrame &input)
{
    actor.sprintingLastTick = input.sprint;
    actor.cameraSteeringLastTick = input.cameraSteering;

    if (input.moveTarget.has_value()) {
        actor.moveTarget = *input.moveTarget;
        actor.hasMoveTarget = true;
    }

    const bool directMovement = hasAxes(input.moveAxes);
    if (directMovement) {
        actor.hasMoveTarget = false;
    }

    actor.state.moveSpeedWorldUnitsPerSecond =
        world.tuning.playerRunSpeedWorldUnitsPerSecond * (input.sprint ? world.tuning.playerSprintSpeedScale : 1.0f);

    if (directMovement) {
        actor.movedLastTick = combat::applyFramedStrafeLocomotion(actor.state, {input.moveAxes},
                                                                  {input.movementYawDegrees}, input.cameraSteering,
                                                                  combat::kFixedTickSeconds, world.arena, world.tuning);
        return;
    }

    if (actor.hasMoveTarget) {
        actor.movedLastTick = combat::applyMoveToWorldTarget(actor.state, actor.moveTarget, combat::kFixedTickSeconds,
                                                             world.arena, world.tuning);
        if (!actor.movedLastTick) {
            actor.hasMoveTarget = false;
        }
        return;
    }

    if (input.cameraSteering) {
        actor.movedLastTick =
            combat::applyFramedStrafeLocomotion(actor.state, {{0.0f, 0.0f}}, {input.movementYawDegrees}, true,
                                                combat::kFixedTickSeconds, world.arena, world.tuning);
        return;
    }

    actor.movedLastTick = false;
}

std::vector<InputFrame> inputsForTick(const std::vector<InputFrame> &inputFrames, uint32_t tick)
{
    std::vector<InputFrame> result;
    for (const InputFrame &input : inputFrames) {
        if (input.tick == tick) {
            result.push_back(input);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const InputFrame &left, const InputFrame &right) { return left.actorId.value < right.actorId.value; });
    return result;
}
} // namespace

RuntimeWorld importRuntimeWorld(const content::RuntimeWorld &contentWorld, const combat::MovementTuning &tuning)
{
    RuntimeWorld world;
    world.worldBounds = contentWorld.worldBounds;
    world.arena = {arenaHalfExtents(contentWorld.worldBounds), tuning.arenaEdgeInsetWorldUnits};
    world.tuning = tuning;

    uint32_t nextActorId = 1;
    for (const content::RuntimeSpawnPoint &spawn : contentWorld.spawnPoints) {
        RuntimeActor actor;
        actor.id = {nextActorId++};
        actor.spawnId = spawn.id;
        actor.team = spawn.team;
        actor.state.currentTransform.translation = spawn.translation;
        actor.state.currentTransform.rotationDegrees.y = spawn.yawDegrees;
        actor.state.previousTransform = actor.state.currentTransform;
        actor.state.moveSpeedWorldUnitsPerSecond = tuning.playerRunSpeedWorldUnitsPerSecond;
        world.actors.push_back(std::move(actor));
    }

    return world;
}

RuntimeActor *findActor(RuntimeWorld &world, RuntimeActorId actorId)
{
    const auto it = std::find_if(world.actors.begin(), world.actors.end(),
                                 [actorId](const RuntimeActor &actor) { return actor.id.value == actorId.value; });
    return it == world.actors.end() ? nullptr : &*it;
}

const RuntimeActor *findActor(const RuntimeWorld &world, RuntimeActorId actorId)
{
    const auto it = std::find_if(world.actors.begin(), world.actors.end(),
                                 [actorId](const RuntimeActor &actor) { return actor.id.value == actorId.value; });
    return it == world.actors.end() ? nullptr : &*it;
}

RuntimeActorId actorIdForSpawn(const RuntimeWorld &world, std::string_view spawnId)
{
    const auto it = std::find_if(world.actors.begin(), world.actors.end(),
                                 [spawnId](const RuntimeActor &actor) { return actor.spawnId == spawnId; });
    if (it == world.actors.end()) {
        return {};
    }
    return it->id;
}

TickResult advanceFixedTick(RuntimeWorld &world, const std::vector<InputFrame> &inputFrames)
{
    for (RuntimeActor &actor : world.actors) {
        combat::beginTick(actor.state);
        actor.movedLastTick = false;
        actor.sprintingLastTick = false;
        actor.cameraSteeringLastTick = false;
    }

    const std::vector<InputFrame> tickInputs = inputsForTick(inputFrames, world.tick);
    for (const InputFrame &input : tickInputs) {
        if (RuntimeActor *actor = findActor(world, input.actorId)) {
            applyInput(world, *actor, input);
        }
    }

    ++world.tick;
    return {world.tick, stateHash(world)};
}

void runFixedTicks(RuntimeWorld &world, const std::vector<InputFrame> &inputFrames, uint32_t tickCount)
{
    for (uint32_t i = 0; i < tickCount; ++i) {
        advanceFixedTick(world, inputFrames);
    }
}

RuntimeWorld runWithRenderCadence(RuntimeWorld world, const std::vector<InputFrame> &inputFrames,
                                  const std::vector<double> &frameDeltasSeconds)
{
    double accumulator = 0.0;
    for (double deltaSeconds : frameDeltasSeconds) {
        accumulator += deltaSeconds;
        while (accumulator + 0.000000001 >= kFixedTickSeconds) {
            advanceFixedTick(world, inputFrames);
            accumulator -= kFixedTickSeconds;
        }

        const float alpha = static_cast<float>(std::clamp(accumulator / kFixedTickSeconds, 0.0, 1.0));
        for (const RuntimeActor &actor : world.actors) {
            (void)combat::interpolate(actor.state, alpha);
        }
    }
    return world;
}

uint64_t stateHash(const RuntimeWorld &world)
{
    uint64_t hash = kFnvOffset;

    // Field order is part of the deterministic simulation contract:
    // world tick, actor count, then actors sorted by runtime id with quantized
    // current transform, movement speed, target state, and last-tick movement flags.
    appendUint32(hash, world.tick);
    appendUint32(hash, static_cast<uint32_t>(world.actors.size()));

    std::vector<const RuntimeActor *> sortedActors;
    sortedActors.reserve(world.actors.size());
    for (const RuntimeActor &actor : world.actors) {
        sortedActors.push_back(&actor);
    }
    std::sort(sortedActors.begin(), sortedActors.end(),
              [](const RuntimeActor *left, const RuntimeActor *right) { return left->id.value < right->id.value; });

    for (const RuntimeActor *actor : sortedActors) {
        appendUint32(hash, actor->id.value);
        appendInt32(hash, quantizeFloat(actor->state.currentTransform.translation.x));
        appendInt32(hash, quantizeFloat(actor->state.currentTransform.translation.y));
        appendInt32(hash, quantizeFloat(actor->state.currentTransform.translation.z));
        appendInt32(hash, quantizeFloat(actor->state.currentTransform.rotationDegrees.y));
        appendInt32(hash, quantizeFloat(actor->state.moveSpeedWorldUnitsPerSecond));
        appendBool(hash, actor->hasMoveTarget);
        appendInt32(hash, quantizeFloat(actor->moveTarget.x));
        appendInt32(hash, quantizeFloat(actor->moveTarget.y));
        appendBool(hash, actor->movedLastTick);
        appendBool(hash, actor->sprintingLastTick);
        appendBool(hash, actor->cameraSteeringLastTick);
    }

    return hash;
}

Transform presentationTransform(const RuntimeWorld &world, RuntimeActorId actorId, float alpha)
{
    const RuntimeActor *actor = findActor(world, actorId);
    if (actor == nullptr) {
        throw std::runtime_error("Cannot sample missing runtime actor");
    }
    return combat::interpolate(actor->state, alpha);
}
} // namespace vac::simulation
