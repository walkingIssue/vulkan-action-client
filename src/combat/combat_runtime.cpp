#include "combat/combat_runtime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vac::combat
{
namespace
{
bool isInRange(uint32_t tick, content::CompiledRange range) { return tick >= range.begin && tick < range.end; }

bool conditionMatches(std::string_view condition, CancelSignal signal)
{
    if (condition == "always") {
        return true;
    }
    if (condition == "on_hit") {
        return signal == CancelSignal::hit;
    }
    if (condition == "on_block") {
        return signal == CancelSignal::block;
    }
    if (condition == "hit_or_block") {
        return signal == CancelSignal::hit || signal == CancelSignal::block;
    }
    if (condition == "whiff") {
        return signal == CancelSignal::whiff;
    }
    return false;
}

CancelSignal contextForActor(const std::vector<CombatTickContext> &contexts, simulation::RuntimeActorId actorId)
{
    for (const CombatTickContext &context : contexts) {
        if (context.actorId.value == actorId.value) {
            return context.cancelSignal;
        }
    }
    return CancelSignal::none;
}

void emitEvent(CombatRuntime &runtime, CombatTickResult &result, CombatActorState &actor, CombatEventKind kind,
               const RuntimeMove &move, std::string label, uint32_t authoredEventId = 0, uint32_t phaseIndex = 0)
{
    result.events.push_back({
        runtime.nextEventSequence++,
        runtime.tick,
        actor.actorId,
        kind,
        move.runtimeMoveId,
        actor.moveInstanceSequence,
        actor.moveTick,
        authoredEventId,
        phaseIndex,
        std::move(label),
    });
}

void startMove(CombatRuntime &runtime, CombatTickResult &result, CombatActorState &actor, const RuntimeMove &move)
{
    actor.activeMoveId = move.runtimeMoveId;
    actor.moveTick = 0;
    actor.moveInstanceSequence = runtime.nextMoveInstanceSequence++;
    actor.stateTags.clear();
    actor.hitRegistry.clear();
    emitEvent(runtime, result, actor, CombatEventKind::moveStarted, move, move.compiled.logicalId);
}

void emitMoveTickEvents(CombatRuntime &runtime, CombatTickResult &result, CombatActorState &actor,
                        const RuntimeMove &move)
{
    for (size_t i = 0; i < move.compiled.phases.size(); ++i) {
        const content::CompiledPhase &phase = move.compiled.phases[i];
        if (phase.range.begin == actor.moveTick) {
            actor.stateTags = phase.tagIds;
            emitEvent(runtime, result, actor, CombatEventKind::phaseEntered, move, phase.id, 0,
                      static_cast<uint32_t>(i + 1));
        }
    }

    for (const content::CompiledMoveEvent &event : move.compiled.events) {
        if (event.tick == actor.moveTick) {
            emitEvent(runtime, result, actor, CombatEventKind::authoredEvent, move, event.type + ":" + event.payload,
                      event.eventId);
        }
    }
}

void ageCommandBuffer(CombatActorState &actor)
{
    for (BufferedCommand &command : actor.commandBuffer) {
        ++command.ageTicks;
    }
    actor.commandBuffer.erase(
        std::remove_if(actor.commandBuffer.begin(), actor.commandBuffer.end(),
                       [](const BufferedCommand &command) { return command.ageTicks > command.bufferTicks; }),
        actor.commandBuffer.end());
}

const RuntimeMove *consumeCommandForMove(CombatActorState &actor, const CombatMoveLibrary &library,
                                         const std::vector<uint32_t> &allowedMoveIds = {})
{
    for (auto it = actor.commandBuffer.begin(); it != actor.commandBuffer.end(); ++it) {
        if (!allowedMoveIds.empty()) {
            for (uint32_t allowedMoveId : allowedMoveIds) {
                const RuntimeMove *move = library.findMove(allowedMoveId);
                if (move != nullptr && move->compiled.input.command == it->command) {
                    actor.commandBuffer.erase(it);
                    return move;
                }
            }
            continue;
        }

        const RuntimeMove *move = library.findByCommand(it->command);
        if (move == nullptr) {
            continue;
        }
        actor.commandBuffer.erase(it);
        return move;
    }
    return nullptr;
}
} // namespace

CombatMoveLibrary::CombatMoveLibrary(std::vector<content::CompiledMove> moves)
{
    std::sort(moves.begin(), moves.end(), [](const content::CompiledMove &left, const content::CompiledMove &right) {
        return left.logicalId < right.logicalId;
    });

    uint32_t nextId = 1;
    for (content::CompiledMove &move : moves) {
        const uint32_t runtimeMoveId = nextId++;
        m_runtimeIdByLogicalId[move.logicalId] = runtimeMoveId;
        m_moves.push_back({runtimeMoveId, std::move(move)});
    }
}

const RuntimeMove *CombatMoveLibrary::findMove(uint32_t runtimeMoveId) const
{
    const auto it = std::find_if(m_moves.begin(), m_moves.end(), [runtimeMoveId](const RuntimeMove &move) {
        return move.runtimeMoveId == runtimeMoveId;
    });
    return it == m_moves.end() ? nullptr : &*it;
}

const RuntimeMove *CombatMoveLibrary::findByLogicalId(std::string_view logicalId) const
{
    const std::optional<uint32_t> runtimeId = runtimeIdForLogicalId(logicalId);
    return runtimeId.has_value() ? findMove(*runtimeId) : nullptr;
}

const RuntimeMove *CombatMoveLibrary::findByCommand(std::string_view command) const
{
    const auto it = std::find_if(m_moves.begin(), m_moves.end(),
                                 [command](const RuntimeMove &move) { return move.compiled.input.command == command; });
    return it == m_moves.end() ? nullptr : &*it;
}

std::optional<uint32_t> CombatMoveLibrary::runtimeIdForLogicalId(std::string_view logicalId) const
{
    const auto it = m_runtimeIdByLogicalId.find(std::string{logicalId});
    if (it == m_runtimeIdByLogicalId.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<uint32_t> CombatMoveLibrary::runtimeDestinationId(const RuntimeMove &sourceMove,
                                                                uint32_t localMoveId) const
{
    if (localMoveId == 0 || localMoveId > sourceMove.compiled.internTable.moveIds.size()) {
        return std::nullopt;
    }
    return runtimeIdForLogicalId(sourceMove.compiled.internTable.moveIds[localMoveId - 1]);
}

CombatRuntime createCombatRuntime(const CombatMoveLibrary &moveLibrary,
                                  const std::vector<simulation::RuntimeActor> &actors)
{
    CombatRuntime runtime{0, moveLibrary};
    for (const simulation::RuntimeActor &actor : actors) {
        runtime.actors.push_back({actor.id});
    }
    return runtime;
}

CombatActorState *findCombatActor(CombatRuntime &runtime, simulation::RuntimeActorId actorId)
{
    const auto it =
        std::find_if(runtime.actors.begin(), runtime.actors.end(),
                     [actorId](const CombatActorState &actor) { return actor.actorId.value == actorId.value; });
    return it == runtime.actors.end() ? nullptr : &*it;
}

const CombatActorState *findCombatActor(const CombatRuntime &runtime, simulation::RuntimeActorId actorId)
{
    const auto it =
        std::find_if(runtime.actors.begin(), runtime.actors.end(),
                     [actorId](const CombatActorState &actor) { return actor.actorId.value == actorId.value; });
    return it == runtime.actors.end() ? nullptr : &*it;
}

CombatTickResult advanceCombatTick(CombatRuntime &runtime, const std::vector<simulation::InputFrame> &inputs,
                                   const std::vector<CombatTickContext> &contexts)
{
    CombatTickResult result;

    for (const simulation::InputFrame &input : inputs) {
        if (input.tick != runtime.tick) {
            continue;
        }
        CombatActorState *actor = findCombatActor(runtime, input.actorId);
        if (actor == nullptr) {
            continue;
        }
        for (const std::string &command : input.pressedCommands) {
            if (const RuntimeMove *move = runtime.moveLibrary.findByCommand(command)) {
                actor->commandBuffer.push_back({command, 0, move->compiled.input.bufferTicks});
            }
        }
    }

    for (CombatActorState &actor : runtime.actors) {
        if (actor.hitstopRemaining > 0) {
            --actor.hitstopRemaining;
            ageCommandBuffer(actor);
            continue;
        }
        if (actor.stunRemaining > 0) {
            --actor.stunRemaining;
            ageCommandBuffer(actor);
            continue;
        }

        if (actor.activeMoveId == 0) {
            if (const RuntimeMove *move = consumeCommandForMove(actor, runtime.moveLibrary)) {
                startMove(runtime, result, actor, *move);
            }
        }

        if (actor.activeMoveId != 0) {
            const RuntimeMove *move = runtime.moveLibrary.findMove(actor.activeMoveId);
            if (move == nullptr) {
                throw std::runtime_error("Combat actor references missing active move");
            }

            emitMoveTickEvents(runtime, result, actor, *move);

            const CancelSignal signal = contextForActor(contexts, actor.actorId);
            bool canceled = false;
            for (const content::CompiledCancelWindow &cancel : move->compiled.cancels) {
                if (!isInRange(actor.moveTick, cancel.range) || !conditionMatches(cancel.condition, signal)) {
                    continue;
                }

                std::vector<uint32_t> allowedRuntimeMoveIds;
                for (uint32_t localMoveId : cancel.destinationMoveIds) {
                    if (const std::optional<uint32_t> runtimeMoveId =
                            runtime.moveLibrary.runtimeDestinationId(*move, localMoveId)) {
                        allowedRuntimeMoveIds.push_back(*runtimeMoveId);
                    }
                }

                if (const RuntimeMove *destination =
                        consumeCommandForMove(actor, runtime.moveLibrary, allowedRuntimeMoveIds)) {
                    emitEvent(runtime, result, actor, CombatEventKind::moveCanceled, *move, cancel.id);
                    startMove(runtime, result, actor, *destination);
                    canceled = true;
                    break;
                }
            }

            if (!canceled) {
                ++actor.moveTick;
                if (actor.moveTick >= move->compiled.durationTicks) {
                    emitEvent(runtime, result, actor, CombatEventKind::moveCompleted, *move, move->compiled.logicalId);
                    actor.activeMoveId = 0;
                    actor.moveTick = 0;
                    actor.stateTags.clear();
                }
            }
        }

        ageCommandBuffer(actor);
    }

    ++runtime.tick;
    return result;
}

void applyHitstop(CombatActorState &actor, uint16_t ticks) { actor.hitstopRemaining = ticks; }

void applyStun(CombatActorState &actor, uint16_t ticks) { actor.stunRemaining = ticks; }
} // namespace vac::combat
