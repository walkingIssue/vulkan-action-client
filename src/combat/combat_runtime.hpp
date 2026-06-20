#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "content/move_asset.hpp"
#include "simulation/runtime_world.hpp"

namespace vac::combat
{
enum class CombatEventKind {
    moveStarted,
    phaseEntered,
    authoredEvent,
    moveCompleted,
    moveCanceled,
};

enum class CancelSignal {
    none,
    hit,
    block,
    whiff,
};

struct CombatEvent
{
    uint32_t eventSequence = 0;
    uint32_t tick = 0;
    simulation::RuntimeActorId actorId;
    CombatEventKind kind = CombatEventKind::moveStarted;
    uint32_t moveId = 0;
    uint32_t moveInstanceSequence = 0;
    uint32_t moveTick = 0;
    uint32_t authoredEventId = 0;
    uint32_t phaseIndex = 0;
    std::string label;
};

struct BufferedCommand
{
    std::string command;
    uint32_t ageTicks = 0;
    uint32_t bufferTicks = 0;
};

struct CombatActorState
{
    simulation::RuntimeActorId actorId;
    uint32_t activeMoveId = 0;
    uint32_t moveInstanceSequence = 0;
    uint16_t moveTick = 0;
    std::vector<uint32_t> stateTags;
    uint16_t hitstopRemaining = 0;
    uint16_t stunRemaining = 0;
    std::vector<BufferedCommand> commandBuffer;
    std::vector<uint32_t> hitRegistry;
};

struct RuntimeMove
{
    uint32_t runtimeMoveId = 0;
    content::CompiledMove compiled;
};

class CombatMoveLibrary
{
  public:
    explicit CombatMoveLibrary(std::vector<content::CompiledMove> moves);

    const RuntimeMove *findMove(uint32_t runtimeMoveId) const;
    const RuntimeMove *findByLogicalId(std::string_view logicalId) const;
    const RuntimeMove *findByCommand(std::string_view command) const;
    std::optional<uint32_t> runtimeIdForLogicalId(std::string_view logicalId) const;
    std::optional<uint32_t> runtimeDestinationId(const RuntimeMove &sourceMove, uint32_t localMoveId) const;
    size_t size() const { return m_moves.size(); }

  private:
    std::vector<RuntimeMove> m_moves;
    std::unordered_map<std::string, uint32_t> m_runtimeIdByLogicalId;
};

struct CombatRuntime
{
    uint32_t tick = 0;
    CombatMoveLibrary moveLibrary;
    std::vector<CombatActorState> actors;
    uint32_t nextMoveInstanceSequence = 1;
    uint32_t nextEventSequence = 1;
};

struct CombatTickContext
{
    simulation::RuntimeActorId actorId;
    CancelSignal cancelSignal = CancelSignal::none;
};

struct CombatTickResult
{
    std::vector<CombatEvent> events;
};

CombatRuntime createCombatRuntime(const CombatMoveLibrary &moveLibrary,
                                  const std::vector<simulation::RuntimeActor> &actors);
CombatActorState *findCombatActor(CombatRuntime &runtime, simulation::RuntimeActorId actorId);
const CombatActorState *findCombatActor(const CombatRuntime &runtime, simulation::RuntimeActorId actorId);
CombatTickResult advanceCombatTick(CombatRuntime &runtime, const std::vector<simulation::InputFrame> &inputs,
                                   const std::vector<CombatTickContext> &contexts = {});
void applyHitstop(CombatActorState &actor, uint16_t ticks);
void applyStun(CombatActorState &actor, uint16_t ticks);
} // namespace vac::combat
