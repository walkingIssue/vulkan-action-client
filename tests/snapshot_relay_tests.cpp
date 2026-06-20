#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "network/snapshot_relay.hpp"
#include "network/state_protocol.hpp"

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

bool hasEventFor(const vac::net::RelayResult &result,
                 std::string_view endpointKey,
                 vac::net::ServerEventKind event,
                 uint8_t clientId)
{
    for (const vac::net::RelayOutput &output : result.outputs) {
        const std::optional<vac::net::ServerEventPacket> decoded = serverEvent(output);
        if (output.endpointKey == endpointKey &&
            decoded.has_value() &&
            decoded->event == event &&
            decoded->clientId == clientId) {
            return true;
        }
    }
    return false;
}
} // namespace

int main()
{
    vac::net::SnapshotRelay relay;

    const vac::net::RelayResult straySnapshot = relay.ingest("endpoint-a", snapshotPacket(1, 1));
    expect(!straySnapshot.accepted, "snapshot before connect is rejected");
    expect(straySnapshot.outputs.empty(), "rejected snapshot produces no fanout");
    expect(relay.clientCount() == 0, "rejected snapshot does not create a session");

    const vac::net::RelayResult firstConnect =
        relay.ingest("endpoint-a", vac::net::encodeConnectPacket({1, 10}));
    expect(firstConnect.accepted, "first connect is accepted");
    expect(firstConnect.outputs.empty(), "first connect has no peers to notify");
    expect(relay.clientCount() == 1, "first connect creates one session");
    expect(relay.hasClient(1), "first connect registers client id");
    expect(relay.endpointForClient(1).value_or("") == "endpoint-a", "client id maps to endpoint");

    const vac::net::RelayResult secondConnect =
        relay.ingest("endpoint-b", vac::net::encodeConnectPacket({2, 20}));
    expect(secondConnect.accepted, "second connect is accepted");
    expect(secondConnect.outputs.size() == 2, "second connect notifies new client and existing peer");
    expect(hasEventFor(secondConnect,
                       "endpoint-a",
                       vac::net::ServerEventKind::clientConnected,
                       2),
           "existing peer receives new-client connect event");
    expect(hasEventFor(secondConnect,
                       "endpoint-b",
                       vac::net::ServerEventKind::clientConnected,
                       1),
           "new client receives existing-peer connect event");

    const vac::net::RelayResult duplicateConnect =
        relay.ingest("endpoint-b", vac::net::encodeConnectPacket({2, 21}));
    expect(duplicateConnect.accepted, "duplicate connect from same endpoint is accepted");
    expect(duplicateConnect.outputs.empty(), "duplicate connect does not fan out another connect event");

    const vac::net::RelayResult wrongEndpointSnapshot =
        relay.ingest("endpoint-b", snapshotPacket(1, 2));
    expect(!wrongEndpointSnapshot.accepted, "endpoint cannot publish another client's snapshot");
    expect(wrongEndpointSnapshot.outputs.empty(), "wrong endpoint snapshot has no fanout");

    const vac::net::RelayResult acceptedSnapshot = relay.ingest("endpoint-a", snapshotPacket(1, 3));
    expect(acceptedSnapshot.accepted, "connected endpoint snapshot is accepted");
    expect(acceptedSnapshot.outputs.size() == 1, "snapshot fans out to one peer");
    expect(!acceptedSnapshot.outputs.empty() && acceptedSnapshot.outputs.front().endpointKey == "endpoint-b",
           "snapshot fanout excludes sender");
    const std::optional<vac::net::ActorSnapshot> forwardedSnapshot =
        acceptedSnapshot.outputs.empty() ? std::nullopt : actorSnapshot(acceptedSnapshot.outputs.front());
    expect(forwardedSnapshot.has_value() && forwardedSnapshot->clientId == 1 && forwardedSnapshot->tick == 3,
           "snapshot fanout keeps original actor payload");

    const vac::net::RelayResult disconnect =
        relay.ingest("endpoint-a", vac::net::encodeDisconnectPacket({1, 30}));
    expect(disconnect.accepted, "disconnect from owning endpoint is accepted");
    expect(relay.clientCount() == 1, "disconnect removes one session");
    expect(!relay.hasClient(1), "disconnected client id is removed");
    expect(hasEventFor(disconnect,
                       "endpoint-b",
                       vac::net::ServerEventKind::clientDisconnected,
                       1),
           "remaining peer receives disconnect event");

    const vac::net::RelayResult postDisconnectSnapshot = relay.ingest("endpoint-a", snapshotPacket(1, 4));
    expect(!postDisconnectSnapshot.accepted, "snapshot after disconnect is rejected");

    return g_failures == 0 ? 0 : 1;
}
