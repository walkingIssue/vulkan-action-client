#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "combat/combat_runtime.hpp"
#include "content/move_asset.hpp"

namespace
{
int g_failures = 0;

std::filesystem::path projectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

vac::content::CompiledMove compileMove(std::string_view fileName)
{
    const vac::content::MoveAsset asset = vac::content::loadMoveAsset(projectRoot() / "content/moves" / fileName);
    const vac::content::MoveCompileResult compiled = vac::content::compileMoveAsset(asset);
    if (!compiled.ok()) {
        throw std::runtime_error("Move fixture failed to compile");
    }
    return compiled.move;
}

vac::combat::CombatMoveLibrary makeLibrary()
{
    std::vector<vac::content::CompiledMove> moves;
    moves.push_back(compileMove("light_attack.move.json"));
    moves.push_back(compileMove("light_followup.move.json"));
    moves.push_back(compileMove("dodge.move.json"));
    moves.push_back(compileMove("hit_reaction.move.json"));
    return vac::combat::CombatMoveLibrary{std::move(moves)};
}

std::vector<vac::simulation::RuntimeActor> oneActor()
{
    vac::simulation::RuntimeActor actor;
    actor.id = {1};
    actor.spawnId = "player_spawn";
    actor.team = "player";
    return {actor};
}

std::vector<vac::simulation::RuntimeActor> twoActors()
{
    vac::simulation::RuntimeActor attacker;
    attacker.id = {1};
    attacker.spawnId = "player_spawn";
    attacker.team = "player";

    vac::simulation::RuntimeActor victim;
    victim.id = {2};
    victim.spawnId = "enemy_spawn";
    victim.team = "enemy";
    return {attacker, victim};
}

vac::combat::CombatRuntime makeRuntime() { return vac::combat::createCombatRuntime(makeLibrary(), oneActor()); }

vac::combat::CombatRuntime makeDuelRuntime()
{
    vac::combat::CombatRuntime runtime = vac::combat::createCombatRuntime(makeLibrary(), twoActors());
    for (vac::combat::CombatActorState &actor : runtime.actors) {
        vac::combat::setActorHealth(actor, 100, 100);
    }
    return runtime;
}

vac::simulation::InputFrame commandAt(uint32_t tick, std::string command)
{
    vac::simulation::InputFrame input;
    input.tick = tick;
    input.actorId = {1};
    input.pressedCommands.push_back(std::move(command));
    return input;
}

std::string activeMoveLogicalId(const vac::combat::CombatRuntime &runtime,
                                vac::simulation::RuntimeActorId actorId = {1})
{
    const vac::combat::CombatActorState *actor = vac::combat::findCombatActor(runtime, actorId);
    if (actor == nullptr || actor->activeMoveId == 0) {
        return {};
    }
    const vac::combat::RuntimeMove *move = runtime.moveLibrary.findMove(actor->activeMoveId);
    return move == nullptr ? std::string{} : move->compiled.logicalId;
}

bool hasEvent(const vac::combat::CombatTickResult &result, vac::combat::CombatEventKind kind, std::string_view label)
{
    for (const vac::combat::CombatEvent &event : result.events) {
        if (event.kind == kind && event.label == label) {
            return true;
        }
    }
    return false;
}

std::string eventSummary(const std::vector<vac::combat::CombatEvent> &events)
{
    std::ostringstream output;
    for (const vac::combat::CombatEvent &event : events) {
        output << event.eventSequence << ':' << static_cast<int>(event.kind) << ':' << event.tick << ':'
               << event.moveTick << ':' << event.authoredEventId << ':' << event.phaseIndex << ':' << event.label
               << ';';
    }
    return output.str();
}

std::vector<vac::combat::CombatEvent> runLightAttackBoundarySequence()
{
    vac::combat::CombatRuntime runtime = makeRuntime();
    std::vector<vac::combat::CombatEvent> events;
    for (uint32_t tick = 0; tick <= 34; ++tick) {
        std::vector<vac::simulation::InputFrame> inputs;
        if (tick == 0) {
            inputs.push_back(commandAt(0, "light_attack"));
        }
        vac::combat::CombatTickResult result = vac::combat::advanceCombatTick(runtime, inputs);
        events.insert(events.end(), result.events.begin(), result.events.end());
    }
    return events;
}

void phaseBoundariesAndAuthoredEvents()
{
    const std::vector<vac::combat::CombatEvent> events = runLightAttackBoundarySequence();
    vac::combat::CombatTickResult all;
    all.events = events;
    expect(hasEvent(all, vac::combat::CombatEventKind::moveStarted, "move.light_attack"), "light attack starts");
    expect(hasEvent(all, vac::combat::CombatEventKind::phaseEntered, "startup"), "startup phase enters");
    expect(hasEvent(all, vac::combat::CombatEventKind::phaseEntered, "active"), "active phase enters");
    expect(hasEvent(all, vac::combat::CombatEventKind::phaseEntered, "recovery"), "recovery phase enters");
    expect(hasEvent(all, vac::combat::CombatEventKind::authoredEvent, "animation_event:sword_swing_start"),
           "authored swing event emits");
    expect(hasEvent(all, vac::combat::CombatEventKind::authoredEvent, "debug_event:blade_arc_on"),
           "authored hitbox debug event emits");
    expect(hasEvent(all, vac::combat::CombatEventKind::moveCompleted, "move.light_attack"), "light attack completes");
}

void bufferedCommandStartsAfterRecovery()
{
    vac::combat::CombatRuntime runtime = makeRuntime();
    for (uint32_t tick = 0; tick <= 34; ++tick) {
        std::vector<vac::simulation::InputFrame> inputs;
        if (tick == 0) {
            inputs.push_back(commandAt(0, "light_attack"));
        }
        if (tick == 30) {
            inputs.push_back(commandAt(30, "dodge"));
        }
        vac::combat::advanceCombatTick(runtime, inputs);
    }
    expect(activeMoveLogicalId(runtime) == "move.dodge", "buffered dodge starts when light attack completes");
}

void cancelOnHitAndNoCancelOnWhiff()
{
    {
        vac::combat::CombatRuntime runtime = makeRuntime();
        for (uint32_t tick = 0; tick <= 12; ++tick) {
            std::vector<vac::simulation::InputFrame> inputs;
            if (tick == 0) {
                inputs.push_back(commandAt(0, "light_attack"));
            }
            if (tick == 12) {
                inputs.push_back(commandAt(12, "light_attack"));
            }
            const std::vector<vac::combat::CombatTickContext> contexts =
                tick == 12 ? std::vector<vac::combat::CombatTickContext>{{{1}, vac::combat::CancelSignal::hit}}
                           : std::vector<vac::combat::CombatTickContext>{};
            vac::combat::advanceCombatTick(runtime, inputs, contexts);
        }
        expect(activeMoveLogicalId(runtime) == "move.light_followup", "cancel-on-hit starts light followup");
    }

    {
        vac::combat::CombatRuntime runtime = makeRuntime();
        for (uint32_t tick = 0; tick <= 12; ++tick) {
            std::vector<vac::simulation::InputFrame> inputs;
            if (tick == 0) {
                inputs.push_back(commandAt(0, "light_attack"));
            }
            if (tick == 12) {
                inputs.push_back(commandAt(12, "light_attack"));
            }
            const std::vector<vac::combat::CombatTickContext> contexts =
                tick == 12 ? std::vector<vac::combat::CombatTickContext>{{{1}, vac::combat::CancelSignal::whiff}}
                           : std::vector<vac::combat::CombatTickContext>{};
            vac::combat::advanceCombatTick(runtime, inputs, contexts);
        }
        expect(activeMoveLogicalId(runtime) == "move.light_attack", "hit-only cancel does not fire on whiff");
    }
}

void authoredDodgeAndHitReactionExist()
{
    const vac::combat::CombatRuntime runtime = makeRuntime();
    expect(runtime.moveLibrary.findByLogicalId("move.dodge") != nullptr, "dodge exists as authored move");
    expect(runtime.moveLibrary.findByLogicalId("move.hit_reaction") != nullptr, "hit reaction exists as authored move");
}

void alwaysDodgeCancelUsesAuthoredMove()
{
    vac::combat::CombatRuntime runtime = makeRuntime();
    for (uint32_t tick = 0; tick <= 18; ++tick) {
        std::vector<vac::simulation::InputFrame> inputs;
        if (tick == 0) {
            inputs.push_back(commandAt(0, "light_attack"));
        }
        if (tick == 18) {
            inputs.push_back(commandAt(18, "dodge"));
        }
        vac::combat::advanceCombatTick(runtime, inputs);
    }
    expect(activeMoveLogicalId(runtime) == "move.dodge", "always cancel transitions to authored dodge");
}

void hitstopAndStunCountersTickDown()
{
    vac::combat::CombatRuntime runtime = makeRuntime();
    vac::combat::CombatActorState *actor = vac::combat::findCombatActor(runtime, {1});
    expect(actor != nullptr, "combat actor exists for hitstop test");
    if (actor == nullptr) {
        return;
    }

    vac::combat::applyHitstop(*actor, 2);
    vac::combat::advanceCombatTick(runtime, {});
    expect(actor->hitstopRemaining == 1, "hitstop decrements first tick");
    vac::combat::advanceCombatTick(runtime, {});
    expect(actor->hitstopRemaining == 0, "hitstop decrements second tick");

    vac::combat::applyStun(*actor, 2);
    vac::combat::advanceCombatTick(runtime, {commandAt(runtime.tick, "light_attack")});
    expect(actor->stunRemaining == 1, "stun decrements first tick");
    expect(actor->activeMoveId == 0, "stunned actor does not start move");
    vac::combat::advanceCombatTick(runtime, {});
    expect(actor->stunRemaining == 0, "stun decrements second tick");
}

void hitEffectAppliesDamageTimingAndReaction()
{
    vac::combat::CombatRuntime runtime = makeDuelRuntime();
    vac::combat::CombatActorState *victim = vac::combat::findCombatActor(runtime, {2});
    const vac::combat::RuntimeMove *attack = runtime.moveLibrary.findByLogicalId("move.light_attack");
    expect(victim != nullptr, "victim exists for hit effect test");
    expect(attack != nullptr && !attack->compiled.hitboxTracks.empty(), "attack hitbox exists for hit effect test");
    if (victim == nullptr || attack == nullptr || attack->compiled.hitboxTracks.empty()) {
        return;
    }

    const vac::combat::CombatHitEffectResult effect =
        vac::combat::applyHitEffect(runtime, *victim, attack->compiled.hitboxTracks.front());
    expect(effect.damageApplied == 12, "hit effect applies authored damage");
    expect(effect.remainingHealth == 88, "hit effect reports remaining health");
    expect(victim->currentHealth == 88, "victim health is reduced");
    expect(effect.hitstopTicks == 1 && victim->hitstopRemaining == 1, "hit effect applies hitstop");
    expect(effect.stunTicks == 3 && victim->stunRemaining == 3, "hit effect applies stun");
    expect(effect.reactionStarted, "hit effect starts reaction");
    expect(activeMoveLogicalId(runtime, {2}) == "move.hit_reaction", "victim starts hit reaction move");
}

void hitEffectDamageClampsAtZero()
{
    vac::combat::CombatRuntime runtime = makeDuelRuntime();
    vac::combat::CombatActorState *victim = vac::combat::findCombatActor(runtime, {2});
    const vac::combat::RuntimeMove *attack = runtime.moveLibrary.findByLogicalId("move.light_attack");
    expect(victim != nullptr && attack != nullptr && !attack->compiled.hitboxTracks.empty(),
           "damage clamp fixture exists");
    if (victim == nullptr || attack == nullptr || attack->compiled.hitboxTracks.empty()) {
        return;
    }

    vac::combat::setActorHealth(*victim, 5, 100);
    const vac::combat::CombatHitEffectResult effect =
        vac::combat::applyHitEffect(runtime, *victim, attack->compiled.hitboxTracks.front());
    expect(effect.damageApplied == 5, "damage clamps to remaining health");
    expect(effect.remainingHealth == 0 && victim->currentHealth == 0, "victim health does not underflow");
}

void eventIdsAreStable()
{
    const std::string first = eventSummary(runLightAttackBoundarySequence());
    const std::string second = eventSummary(runLightAttackBoundarySequence());
    expect(first == second, "combat event sequence is stable across repeated runs");
}
} // namespace

int main()
{
    try {
        phaseBoundariesAndAuthoredEvents();
        bufferedCommandStartsAfterRecovery();
        cancelOnHitAndNoCancelOnWhiff();
        authoredDodgeAndHitReactionExist();
        alwaysDodgeCancelUsesAuthoredMove();
        hitstopAndStunCountersTickDown();
        hitEffectAppliesDamageTimingAndReaction();
        hitEffectDamageClampsAtZero();
        eventIdsAreStable();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
