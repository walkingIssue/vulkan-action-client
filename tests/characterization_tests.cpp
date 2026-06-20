#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "combat/combat_simulation.hpp"
#include "network/snapshot_relay.hpp"
#include "network/state_protocol.hpp"
#include "scene/scene_runtime.hpp"

namespace
{
using json = nlohmann::json;

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
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::cerr << "FAIL: " << message << " expected " << expected << " got " << actual << '\n';
    }
}

std::filesystem::path fixturePath(std::string_view relative)
{
    return vac::defaultProjectRoot() / "tests" / "fixtures" / "characterization" / relative;
}

json readJson(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open fixture " + path.string());
    }

    json document;
    input >> document;
    return document;
}

glm::vec3 readVec3(const json &value)
{
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

glm::vec2 readVec2(const json &value)
{
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
    };
}

void expectVec3(glm::vec3 actual, glm::vec3 expected, float tolerance, std::string_view message)
{
    expectNear(actual.x, expected.x, tolerance, std::string{message} + " x");
    expectNear(actual.y, expected.y, tolerance, std::string{message} + " y");
    expectNear(actual.z, expected.z, tolerance, std::string{message} + " z");
}

void expectVec2(glm::vec2 actual, glm::vec2 expected, float tolerance, std::string_view message)
{
    expectNear(actual.x, expected.x, tolerance, std::string{message} + " x");
    expectNear(actual.y, expected.y, tolerance, std::string{message} + " y");
}

void expectBounds(const vac::Bounds &actual, const json &expected, std::string_view message)
{
    expect(actual.valid, std::string{message} + " is valid");
    expectVec3(actual.min, readVec3(expected.at("min")), 0.002f, std::string{message} + " min");
    expectVec3(actual.max, readVec3(expected.at("max")), 0.002f, std::string{message} + " max");
}

void expectTransform(const vac::Transform &actual, const json &expected, std::string_view message)
{
    expectVec3(actual.translation, readVec3(expected.at("translation")), 0.001f, std::string{message} + " translation");
    expectVec3(actual.rotationDegrees, readVec3(expected.at("rotation_degrees")), 0.001f,
               std::string{message} + " rotation");
    expectVec3(actual.scale, readVec3(expected.at("scale")), 0.001f, std::string{message} + " scale");
}

std::string packetHex(const std::vector<std::byte> &packet)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = 0; i < packet.size(); ++i) {
        if (i > 0) {
            output << ' ';
        }
        output << std::setw(2) << std::to_integer<unsigned int>(packet[i]);
    }
    return output.str();
}

std::optional<vac::net::ServerEventPacket> serverEvent(const vac::net::RelayOutput &output)
{
    const std::optional<vac::net::Packet> packet = vac::net::decodePacket(output.packet);
    if (!packet.has_value()) {
        return std::nullopt;
    }

    if (const auto *event = std::get_if<vac::net::ServerEventPacket>(&*packet)) {
        return *event;
    }

    return std::nullopt;
}

std::optional<vac::net::ActorSnapshot> actorSnapshot(const vac::net::RelayOutput &output)
{
    const std::optional<vac::net::Packet> packet = vac::net::decodePacket(output.packet);
    if (!packet.has_value()) {
        return std::nullopt;
    }

    if (const auto *snapshot = std::get_if<vac::net::ActorSnapshot>(&*packet)) {
        return *snapshot;
    }

    return std::nullopt;
}

bool hasEventFor(const vac::net::RelayResult &result, std::string_view endpointKey, vac::net::ServerEventKind event,
                 uint8_t clientId)
{
    for (const vac::net::RelayOutput &output : result.outputs) {
        const std::optional<vac::net::ServerEventPacket> decoded = serverEvent(output);
        if (output.endpointKey == endpointKey && decoded.has_value() && decoded->event == event &&
            decoded->clientId == clientId) {
            return true;
        }
    }
    return false;
}

bool hasSnapshotFor(const vac::net::RelayResult &result, std::string_view endpointKey, uint8_t clientId, uint16_t tick)
{
    for (const vac::net::RelayOutput &output : result.outputs) {
        const std::optional<vac::net::ActorSnapshot> decoded = actorSnapshot(output);
        if (output.endpointKey == endpointKey && decoded.has_value() && decoded->clientId == clientId &&
            decoded->tick == tick) {
            return true;
        }
    }
    return false;
}

std::vector<std::byte> snapshotPacket(uint8_t clientId, uint16_t tick)
{
    return vac::net::encodeActorSnapshot({
        clientId,
        tick,
        vac::net::snapshotMoving,
        {static_cast<float>(clientId), 0.0f, static_cast<float>(tick)},
        90.0f,
    });
}

void characterizeBootstrapScene()
{
    const json expected = readJson(fixturePath("bootstrap_scene_expected.json"));
    const vac::SceneRuntime scene =
        vac::loadScene(vac::defaultProjectRoot() / "config/scenes/bootstrap.scene.json", vac::defaultProjectRoot());

    expect(scene.name == expected.at("name").get<std::string>(), "bootstrap scene name matches fixture");
    expect(scene.models.size() == expected.at("models").size(), "bootstrap scene model count matches fixture");
    expect(scene.procedural.size() == expected.at("procedural").size(),
           "bootstrap scene procedural count matches fixture");
    expect(scene.instances.size() == expected.at("instances").size(), "bootstrap scene instance count matches fixture");

    for (const json &expectedModel : expected.at("models")) {
        const std::string id = expectedModel.at("id").get<std::string>();
        const auto modelIt = scene.models.find(id);
        expect(modelIt != scene.models.end(), "expected model exists: " + id);
        if (modelIt == scene.models.end()) {
            continue;
        }

        const vac::ModelStats &model = modelIt->second;
        expect(model.meshCount == expectedModel.at("mesh_count").get<uint32_t>(), "model mesh count matches fixture");
        expect(model.materialCount == expectedModel.at("material_count").get<uint32_t>(),
               "model material count matches fixture");
        expect(model.animationCount == expectedModel.at("animation_count").get<uint32_t>(),
               "model animation count matches fixture");
        expect(model.vertexCount == expectedModel.at("vertex_count").get<uint64_t>(),
               "model vertex count matches fixture");
        expect(model.triangleCount == expectedModel.at("triangle_count").get<uint64_t>(),
               "model triangle count matches fixture");
        expectBounds(model.localBounds, expectedModel.at("local_bounds"), "model local bounds");
    }

    for (size_t i = 0; i < expected.at("procedural").size() && i < scene.procedural.size(); ++i) {
        const json &expectedProcedural = expected.at("procedural").at(i);
        const vac::ProceduralInstance &actual = scene.procedural.at(i);
        expect(actual.id == expectedProcedural.at("id").get<std::string>(), "procedural id matches fixture");
        expect(actual.name == expectedProcedural.at("name").get<std::string>(), "procedural name matches fixture");
        expect(actual.type == expectedProcedural.at("type").get<std::string>(), "procedural type matches fixture");
        expectVec2(actual.size, readVec2(expectedProcedural.at("size")), 0.001f, "procedural size");
        expectTransform(actual.transform, expectedProcedural.at("transform"), "procedural transform");
        expectBounds(actual.worldBounds, expectedProcedural.at("world_bounds"), "procedural world bounds");
    }

    for (size_t i = 0; i < expected.at("instances").size() && i < scene.instances.size(); ++i) {
        const json &expectedInstance = expected.at("instances").at(i);
        const vac::SceneInstance &actual = scene.instances.at(i);
        expect(actual.id == expectedInstance.at("id").get<std::string>(), "instance id matches fixture");
        expect(actual.name == expectedInstance.at("name").get<std::string>(), "instance name matches fixture");
        expect(actual.modelId == expectedInstance.at("model").get<std::string>(), "instance model id matches fixture");
        expectTransform(actual.transform, expectedInstance.at("transform"), "instance transform");
        expectBounds(actual.worldBounds, expectedInstance.at("world_bounds"), "instance world bounds");
    }

    expectBounds(scene.worldBounds, expected.at("world_bounds"), "scene world bounds");
}

void characterizeMovementTraceAndInterpolation()
{
    vac::combat::ActorState actor;
    actor.currentTransform.translation = {0.0f, 0.0f, 0.0f};
    actor.currentTransform.rotationDegrees = {0.0f, 0.0f, 0.0f};
    actor.previousTransform = actor.currentTransform;
    actor.moveSpeedWorldUnitsPerSecond = vac::combat::kDefaultMovementTuning.playerRunSpeedWorldUnitsPerSecond;

    const vac::combat::ArenaLimits arena{{1000.0f, 1000.0f}, 0.0f};
    const float tickSeconds = vac::combat::kFixedTickSeconds;

    for (int tick = 0; tick < 10; ++tick) {
        vac::combat::beginTick(actor);
        const bool moved =
            vac::combat::applyFramedStrafeLocomotion(actor, {{0.0f, 1.0f}}, {30.0f}, true, tickSeconds, arena);
        expect(moved, "movement trace forward tick moves");
    }

    for (int tick = 0; tick < 6; ++tick) {
        vac::combat::beginTick(actor);
        const bool moved =
            vac::combat::applyFramedStrafeLocomotion(actor, {{0.0f, -1.0f}}, {30.0f}, false, tickSeconds, arena);
        expect(moved, "movement trace backpedal tick moves");
    }

    expectVec3(actor.currentTransform.translation, {2.51333f, 0.0f, 4.35322f}, 0.001f,
               "movement trace final translation");
    expectNear(actor.currentTransform.rotationDegrees.y, 30.0f, 0.001f, "movement trace final yaw");
    expectVec3(actor.previousTransform.translation, {2.61806f, 0.0f, 4.53461f}, 0.001f,
               "movement trace previous translation");

    const vac::Transform interpolated = vac::combat::interpolate(actor, 0.5f);
    expectVec3(interpolated.translation, {2.56569f, 0.0f, 4.44391f}, 0.001f, "movement trace interpolated translation");

    actor.previousTransform.translation = {-10.0f, 0.0f, -10.0f};
    actor.currentTransform.translation = {10.0f, 0.0f, 10.0f};
    expectVec3(vac::combat::interpolate(actor, -1.0f).translation, actor.previousTransform.translation, 0.001f,
               "interpolation clamps below zero");
    expectVec3(vac::combat::interpolate(actor, 2.0f).translation, actor.currentTransform.translation, 0.001f,
               "interpolation clamps above one");
}

void characterizeProtocolBytes()
{
    const json expected = readJson(fixturePath("protocol_packets_expected.json"));

    expect(packetHex(vac::net::encodeConnectPacket({3, 42})) == expected.at("connect_client3_seq42").get<std::string>(),
           "connect packet bytes match fixture");
    expect(packetHex(vac::net::encodeDisconnectPacket({3, 43})) ==
               expected.at("disconnect_client3_seq43").get<std::string>(),
           "disconnect packet bytes match fixture");
    expect(packetHex(vac::net::encodeServerEventPacket({vac::net::ServerEventKind::clientDisconnected, 9, 44})) ==
               expected.at("server_client9_disconnected_seq44").get<std::string>(),
           "server event packet bytes match fixture");
    expect(packetHex(vac::net::encodeActorSnapshot({
               7,
               123,
               static_cast<uint8_t>(vac::net::snapshotMoving | vac::net::snapshotSprinting),
               {12.34f, -5.67f, 8.91f},
               271.25f,
           })) == expected.at("snapshot_client7_tick123_moving_sprinting").get<std::string>(),
           "actor snapshot packet bytes match fixture");
}

void characterizeRelayWorldState()
{
    vac::net::SnapshotRelay relay;

    const vac::net::RelayResult firstConnect = relay.ingest("endpoint-a", vac::net::encodeConnectPacket({1, 10}));
    expect(firstConnect.accepted, "relay accepts first connect");

    const vac::net::RelayResult snapshot = relay.ingest("endpoint-a", snapshotPacket(1, 3));
    expect(snapshot.accepted, "relay accepts connected actor snapshot");
    expect(relay.latestSnapshotForClient(1).has_value(), "relay caches connected actor snapshot");

    const vac::net::RelayResult secondConnect = relay.ingest("endpoint-b", vac::net::encodeConnectPacket({2, 20}));
    expect(secondConnect.accepted, "relay accepts late connect");
    expect(hasEventFor(secondConnect, "endpoint-b", vac::net::ServerEventKind::clientConnected, 1),
           "late client receives existing-client event");
    expect(hasEventFor(secondConnect, "endpoint-a", vac::net::ServerEventKind::clientConnected, 2),
           "existing client receives late-client event");
    expect(hasSnapshotFor(secondConnect, "endpoint-b", 1, 3), "late client receives cached actor snapshot");

    const vac::net::RelayResult disconnect = relay.ingest("endpoint-a", vac::net::encodeDisconnectPacket({1, 30}));
    expect(disconnect.accepted, "relay accepts owner disconnect");
    expect(!relay.hasClient(1), "relay removes disconnected client");
    expect(!relay.latestSnapshotForClient(1).has_value(), "relay removes disconnected client snapshot");
    expect(hasEventFor(disconnect, "endpoint-b", vac::net::ServerEventKind::clientDisconnected, 1),
           "remaining peer receives disconnect event");
}
} // namespace

int main()
{
    try {
        characterizeBootstrapScene();
        characterizeMovementTraceAndInterpolation();
        characterizeProtocolBytes();
        characterizeRelayWorldState();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
