#include <cmath>
#include <iostream>
#include <string_view>

#include <glm/geometric.hpp>

#include "combat/combat_simulation.hpp"

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

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    expect(std::fabs(actual - expected) <= tolerance, message);
}

vac::combat::ActorState makeActor(float speed, glm::vec3 translation = glm::vec3{0.0f}, float yawDegrees = 0.0f)
{
    vac::combat::ActorState actor;
    actor.currentTransform.translation = translation;
    actor.currentTransform.rotationDegrees.y = yawDegrees;
    actor.previousTransform = actor.currentTransform;
    actor.moveSpeedWorldUnitsPerSecond = speed;
    return actor;
}

vac::combat::ArenaLimits wideArena()
{
    return {{1000.0f, 1000.0f}, 0.0f};
}
} // namespace

int main()
{
    vac::combat::MovementTuning tuning;

    {
        vac::combat::ActorState actor = makeActor(60.0f);
        const bool moved = vac::combat::applyFramedStrafeLocomotion(actor,
                                                                    {{1.0f, 1.0f}},
                                                                    {0.0f},
                                                                    false,
                                                                    1.0f,
                                                                    wideArena(),
                                                                    tuning);
        const glm::vec2 displacement{actor.currentTransform.translation.x, actor.currentTransform.translation.z};
        expect(moved, "diagonal framed strafe moves");
        expectNear(glm::length(displacement), 60.0f, 0.001f, "diagonal movement is normalized to run speed");
        expectNear(actor.currentTransform.translation.x, 42.4264f, 0.001f, "diagonal movement has right component");
        expectNear(actor.currentTransform.translation.z, 42.4264f, 0.001f, "diagonal movement has forward component");
    }

    {
        vac::combat::ActorState actor = makeActor(60.0f);
        tuning.backpedalSpeedScale = 1.0f / 3.0f;
        const bool moved = vac::combat::applyFramedStrafeLocomotion(actor,
                                                                    {{0.0f, -1.0f}},
                                                                    {0.0f},
                                                                    false,
                                                                    1.0f,
                                                                    wideArena(),
                                                                    tuning);
        expect(moved, "backpedal moves");
        expectNear(actor.currentTransform.translation.x, 0.0f, 0.001f, "backpedal keeps x for yaw zero");
        expectNear(actor.currentTransform.translation.z, -20.0f, 0.001f, "backpedal uses configured speed scale");
        expectNear(actor.currentTransform.rotationDegrees.y, 0.0f, 0.001f, "unlocked backpedal does not rotate actor");
    }

    {
        vac::combat::ActorState actor = makeActor(60.0f, glm::vec3{0.0f}, 180.0f);
        const bool moved = vac::combat::applyFramedStrafeLocomotion(actor,
                                                                    {{0.0f, 1.0f}},
                                                                    {45.0f},
                                                                    true,
                                                                    1.0f,
                                                                    wideArena(),
                                                                    tuning);
        expect(moved, "camera-locked forward movement moves");
        expectNear(actor.currentTransform.rotationDegrees.y, 45.0f, 0.001f, "camera lock snaps facing to movement frame");
        expectNear(actor.currentTransform.translation.x, 42.4264f, 0.001f, "camera-locked movement uses frame forward x");
        expectNear(actor.currentTransform.translation.z, 42.4264f, 0.001f, "camera-locked movement uses frame forward z");
    }

    {
        vac::combat::ActorState actor = makeActor(60.0f, glm::vec3{0.0f}, 180.0f);
        const bool moved = vac::combat::applyFramedStrafeLocomotion(actor,
                                                                    {{0.0f, 0.0f}},
                                                                    {45.0f},
                                                                    true,
                                                                    1.0f,
                                                                    wideArena(),
                                                                    tuning);
        expect(moved, "camera lock reports facing-only rotation");
        expectNear(actor.currentTransform.rotationDegrees.y, 45.0f, 0.001f, "camera lock rotates even without movement input");
        expectNear(actor.currentTransform.translation.x, 0.0f, 0.001f, "facing-only rotation keeps x");
        expectNear(actor.currentTransform.translation.z, 0.0f, 0.001f, "facing-only rotation keeps z");
    }

    {
        vac::combat::ActorState actor = makeActor(10.0f);
        const bool moved = vac::combat::applyMoveToWorldTarget(actor,
                                                               {10.0f, 0.0f},
                                                               0.5f,
                                                               wideArena(),
                                                               tuning);
        expect(moved, "world target movement advances actor");
        expectNear(actor.currentTransform.translation.x, 5.0f, 0.001f, "world target movement steps toward target");
        expectNear(actor.currentTransform.translation.z, 0.0f, 0.001f, "world target movement keeps z on x-axis target");
        expectNear(actor.currentTransform.rotationDegrees.y, 90.0f, 0.001f, "world target movement faces target");
    }

    {
        vac::combat::ActorState actor = makeActor(10.0f);
        tuning.moveTargetArrivalRadiusWorldUnits = 1.0f;
        const bool moved = vac::combat::applyMoveToWorldTarget(actor,
                                                               {0.5f, 0.0f},
                                                               1.0f,
                                                               wideArena(),
                                                               tuning);
        expect(!moved, "world target inside arrival radius does not move");
        expectNear(actor.currentTransform.translation.x, 0.0f, 0.001f, "arrival radius keeps x");
        expectNear(actor.currentTransform.rotationDegrees.y, 0.0f, 0.001f, "arrival radius keeps facing");
    }

    {
        vac::combat::ActorState actor = makeActor(100.0f, {7.0f, 0.0f, 0.0f});
        const vac::combat::ArenaLimits arena{{10.0f, 10.0f}, 3.0f};
        const bool moved = vac::combat::applyMoveToWorldTarget(actor,
                                                               {20.0f, 0.0f},
                                                               1.0f,
                                                               arena,
                                                               tuning);
        expect(moved, "movement into arena edge still reports attempted movement");
        expectNear(actor.currentTransform.translation.x, 7.0f, 0.001f, "arena inset clamps x at allowed edge");
        expectNear(actor.currentTransform.translation.z, 0.0f, 0.001f, "arena clamp keeps z");
    }

    {
        vac::combat::ActorState actor = makeActor(10.0f);
        actor.previousTransform.translation = {0.0f, 0.0f, 0.0f};
        actor.currentTransform.translation = {10.0f, 0.0f, 20.0f};
        actor.previousTransform.rotationDegrees = {0.0f, 0.0f, 0.0f};
        actor.currentTransform.rotationDegrees = {0.0f, 90.0f, 0.0f};
        const vac::Transform interpolated = vac::combat::interpolate(actor, 0.25f);
        expectNear(interpolated.translation.x, 2.5f, 0.001f, "interpolation blends translation x");
        expectNear(interpolated.translation.z, 5.0f, 0.001f, "interpolation blends translation z");
        expectNear(interpolated.rotationDegrees.y, 22.5f, 0.001f, "interpolation blends yaw");
    }

    return g_failures == 0 ? 0 : 1;
}
