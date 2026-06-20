#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

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

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    expect(std::fabs(actual - expected) <= tolerance, message);
}
} // namespace

int main()
{
    const vac::net::ConnectPacket connect{3, 42};
    const std::optional<vac::net::Packet> decodedConnect =
        vac::net::decodePacket(vac::net::encodeConnectPacket(connect));
    const auto *connectRoundTrip = decodedConnect ? std::get_if<vac::net::ConnectPacket>(&*decodedConnect) : nullptr;
    expect(connectRoundTrip != nullptr, "connect packet decodes as connect");
    expect(connectRoundTrip != nullptr && connectRoundTrip->clientId == 3, "connect client id round-trips");
    expect(connectRoundTrip != nullptr && connectRoundTrip->sequence == 42, "connect sequence round-trips");

    const vac::net::DisconnectPacket disconnect{3, 43};
    const std::optional<vac::net::Packet> decodedDisconnect =
        vac::net::decodePacket(vac::net::encodeDisconnectPacket(disconnect));
    const auto *disconnectRoundTrip =
        decodedDisconnect ? std::get_if<vac::net::DisconnectPacket>(&*decodedDisconnect) : nullptr;
    expect(disconnectRoundTrip != nullptr, "disconnect packet decodes as disconnect");
    expect(disconnectRoundTrip != nullptr && disconnectRoundTrip->clientId == 3, "disconnect client id round-trips");
    expect(disconnectRoundTrip != nullptr && disconnectRoundTrip->sequence == 43, "disconnect sequence round-trips");

    const vac::net::ServerEventPacket event{vac::net::ServerEventKind::clientDisconnected, 9, 44};
    const std::optional<vac::net::Packet> decodedEvent =
        vac::net::decodePacket(vac::net::encodeServerEventPacket(event));
    const auto *eventRoundTrip = decodedEvent ? std::get_if<vac::net::ServerEventPacket>(&*decodedEvent) : nullptr;
    expect(eventRoundTrip != nullptr, "server event decodes as server event");
    expect(eventRoundTrip != nullptr && eventRoundTrip->event == vac::net::ServerEventKind::clientDisconnected,
           "server event kind round-trips");
    expect(eventRoundTrip != nullptr && eventRoundTrip->clientId == 9, "server event client id round-trips");
    expect(eventRoundTrip != nullptr && eventRoundTrip->sequence == 44, "server event sequence round-trips");

    const vac::net::ActorSnapshot snapshot{
        7,
        123,
        static_cast<uint8_t>(vac::net::snapshotMoving | vac::net::snapshotSprinting),
        {12.34f, -5.67f, 8.91f},
        271.25f,
    };
    const std::vector<std::byte> encodedSnapshot = vac::net::encodeActorSnapshot(snapshot);
    const std::optional<vac::net::Packet> decodedSnapshot = vac::net::decodePacket(encodedSnapshot);
    const auto *snapshotRoundTrip =
        decodedSnapshot ? std::get_if<vac::net::ActorSnapshot>(&*decodedSnapshot) : nullptr;
    expect(snapshotRoundTrip != nullptr, "actor snapshot decodes as snapshot");
    expect(snapshotRoundTrip != nullptr && snapshotRoundTrip->clientId == 7, "snapshot client id round-trips");
    expect(snapshotRoundTrip != nullptr && snapshotRoundTrip->tick == 123, "snapshot tick round-trips");
    expect(snapshotRoundTrip != nullptr && snapshotRoundTrip->flags == snapshot.flags, "snapshot flags round-trip");
    if (snapshotRoundTrip != nullptr) {
        expectNear(snapshotRoundTrip->position.x, snapshot.position.x, 0.01f, "snapshot x position quantizes");
        expectNear(snapshotRoundTrip->position.y, snapshot.position.y, 0.01f, "snapshot y position quantizes");
        expectNear(snapshotRoundTrip->position.z, snapshot.position.z, 0.01f, "snapshot z position quantizes");
        expectNear(snapshotRoundTrip->yawDegrees, snapshot.yawDegrees, 0.01f, "snapshot yaw quantizes");
    }

    const std::optional<vac::net::ActorSnapshot> legacySnapshot =
        vac::net::decodeActorSnapshot(encodedSnapshot);
    expect(legacySnapshot.has_value(), "decodeActorSnapshot accepts actor snapshots");
    expect(!vac::net::decodeActorSnapshot(vac::net::encodeConnectPacket(connect)).has_value(),
           "decodeActorSnapshot rejects connect packets");

    std::vector<std::byte> invalidMagic = encodedSnapshot;
    invalidMagic[0] = std::byte{0xff};
    expect(!vac::net::decodePacket(invalidMagic).has_value(), "invalid magic is rejected");

    return g_failures == 0 ? 0 : 1;
}
