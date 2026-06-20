#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/authoring_scene.hpp"
#include "simulation/runtime_world.hpp"

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

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::cerr << "FAIL: " << message << " expected " << expected << " got " << actual << '\n';
    }
}

void expectVec3(glm::vec3 actual, glm::vec3 expected, float tolerance, std::string_view message)
{
    expectNear(actual.x, expected.x, tolerance, std::string{message} + " x");
    expectNear(actual.y, expected.y, tolerance, std::string{message} + " y");
    expectNear(actual.z, expected.z, tolerance, std::string{message} + " z");
}

vac::simulation::RuntimeWorld loadSimulationWorld()
{
    const vac::content::AuthoringScene scene =
        vac::content::loadAuthoringScene(projectRoot() / "content/maps/basic_primitive_arena.map.json");
    const vac::content::CompileResult compiled = vac::content::compileRuntimeWorld(scene);
    if (!compiled.ok()) {
        throw std::runtime_error("basic primitive arena failed to compile");
    }

    vac::combat::MovementTuning tuning = vac::combat::kDefaultMovementTuning;
    tuning.playerRunSpeedWorldUnitsPerSecond = 6.0f;
    tuning.playerSprintSpeedScale = 2.0f;
    tuning.arenaEdgeInsetWorldUnits = 0.0f;
    return vac::simulation::importRuntimeWorld(compiled.world, tuning);
}

std::vector<vac::simulation::InputFrame> oneSecondInput(const vac::simulation::RuntimeWorld &world)
{
    const vac::simulation::RuntimeActorId player = vac::simulation::actorIdForSpawn(world, "player_spawn");
    std::vector<vac::simulation::InputFrame> inputs;
    inputs.reserve(60);
    for (uint32_t tick = 0; tick < 60; ++tick) {
        inputs.push_back({
            tick,
            player,
            {0.0f, 1.0f},
            0.0f,
            true,
            tick >= 20 && tick < 30,
            std::nullopt,
        });
    }
    return inputs;
}

std::vector<double> repeatedFrames(int frameCount, double deltaSeconds)
{
    return std::vector<double>(static_cast<size_t>(frameCount), deltaSeconds);
}

std::vector<double> irregularOneSecondFrames()
{
    std::vector<double> frames;
    frames.reserve(100);
    for (int i = 0; i < 50; ++i) {
        frames.push_back(0.007);
        frames.push_back(0.013);
    }
    return frames;
}

uint64_t runHash(vac::simulation::RuntimeWorld world, const std::vector<vac::simulation::InputFrame> &inputs)
{
    vac::simulation::runFixedTicks(world, inputs, 60);
    return vac::simulation::stateHash(world);
}

void importsContentRuntimeWorld()
{
    const vac::simulation::RuntimeWorld world = loadSimulationWorld();
    expect(world.tick == 0, "imported simulation world starts at tick zero");
    expect(world.actors.size() == 2, "imported simulation world creates one actor per enabled spawn");
    expect(vac::simulation::actorIdForSpawn(world, "enemy_spawn").value == 1, "enemy spawn actor id is stable");
    expect(vac::simulation::actorIdForSpawn(world, "player_spawn").value == 2, "player spawn actor id is stable");

    const vac::simulation::RuntimeActor *player = vac::simulation::findActor(world, {2});
    expect(player != nullptr, "player actor exists");
    if (player != nullptr) {
        expectVec3(player->state.currentTransform.translation, {0.0f, 0.0f, 8.0f}, 0.001f,
                   "player actor imported transform");
        expectNear(player->state.currentTransform.rotationDegrees.y, 180.0f, 0.001f, "player actor imported yaw");
    }
}

void repeatedInputStreamProducesSameHash()
{
    const vac::simulation::RuntimeWorld initial = loadSimulationWorld();
    const std::vector<vac::simulation::InputFrame> inputs = oneSecondInput(initial);

    const uint64_t initialHash = vac::simulation::stateHash(initial);
    const uint64_t firstHash = runHash(initial, inputs);
    const uint64_t secondHash = runHash(initial, inputs);

    std::vector<vac::simulation::InputFrame> reversedInputs = inputs;
    std::reverse(reversedInputs.begin(), reversedInputs.end());
    const uint64_t reversedOrderHash = runHash(initial, reversedInputs);

    expect(firstHash != 0, "simulation hash is nonzero");
    expect(firstHash != initialHash, "simulation hash changes after movement");
    expect(firstHash == secondHash, "same input stream produces same hash");
    expect(firstHash == reversedOrderHash, "input frame storage order does not affect hash");
}

void renderCadenceDoesNotChangeSimulationHash()
{
    const vac::simulation::RuntimeWorld initial = loadSimulationWorld();
    const std::vector<vac::simulation::InputFrame> inputs = oneSecondInput(initial);

    const uint64_t fixedHash = runHash(initial, inputs);
    const uint64_t hash30 = vac::simulation::stateHash(
        vac::simulation::runWithRenderCadence(initial, inputs, repeatedFrames(30, 1.0 / 30.0)));
    const uint64_t hash60 = vac::simulation::stateHash(
        vac::simulation::runWithRenderCadence(initial, inputs, repeatedFrames(60, 1.0 / 60.0)));
    const uint64_t hash144 = vac::simulation::stateHash(
        vac::simulation::runWithRenderCadence(initial, inputs, repeatedFrames(144, 1.0 / 144.0)));
    const uint64_t irregularHash =
        vac::simulation::stateHash(vac::simulation::runWithRenderCadence(initial, inputs, irregularOneSecondFrames()));

    expect(hash30 == fixedHash, "30fps render cadence keeps simulation hash");
    expect(hash60 == fixedHash, "60fps render cadence keeps simulation hash");
    expect(hash144 == fixedHash, "144fps render cadence keeps simulation hash");
    expect(irregularHash == fixedHash, "irregular render cadence keeps simulation hash");
}

void presentationInterpolationIsRendererIndependent()
{
    vac::simulation::RuntimeWorld world = loadSimulationWorld();
    const vac::simulation::RuntimeActorId player = vac::simulation::actorIdForSpawn(world, "player_spawn");
    vac::simulation::RuntimeActor *actor = vac::simulation::findActor(world, player);
    expect(actor != nullptr, "presentation interpolation test actor exists");
    if (actor == nullptr) {
        return;
    }

    actor->state.previousTransform.translation = {0.0f, 0.0f, 0.0f};
    actor->state.currentTransform.translation = {10.0f, 0.0f, 20.0f};
    actor->state.previousTransform.rotationDegrees = {0.0f, 0.0f, 0.0f};
    actor->state.currentTransform.rotationDegrees = {0.0f, 90.0f, 0.0f};

    const vac::Transform midpoint = vac::simulation::presentationTransform(world, player, 0.5f);
    expectVec3(midpoint.translation, {5.0f, 0.0f, 10.0f}, 0.001f, "presentation midpoint translation");
    expectNear(midpoint.rotationDegrees.y, 45.0f, 0.001f, "presentation midpoint yaw");

    expectVec3(vac::simulation::presentationTransform(world, player, -1.0f).translation,
               actor->state.previousTransform.translation, 0.001f, "presentation interpolation clamps below zero");
    expectVec3(vac::simulation::presentationTransform(world, player, 2.0f).translation,
               actor->state.currentTransform.translation, 0.001f, "presentation interpolation clamps above one");
}
} // namespace

int main()
{
    try {
        importsContentRuntimeWorld();
        repeatedInputStreamProducesSameHash();
        renderCadenceDoesNotChangeSimulationHash();
        presentationInterpolationIsRendererIndependent();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
