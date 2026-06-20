#include "network/snapshot_client.hpp"

#include <span>

namespace vac::net
{
SnapshotClient::SnapshotClient(uint8_t clientId, Endpoint serverEndpoint)
    : m_clientId(clientId),
      m_serverEndpoint(serverEndpoint)
{
    m_socket.open("0.0.0.0", 0, true);
}

void SnapshotClient::sendSnapshot(ActorSnapshot snapshot)
{
    snapshot.clientId = m_clientId;
    const std::vector<std::byte> packet = encodeActorSnapshot(snapshot);
    m_socket.sendTo(packet, m_serverEndpoint);
}

std::vector<ActorSnapshot> SnapshotClient::receiveSnapshots()
{
    std::vector<ActorSnapshot> snapshots;
    while (true) {
        std::optional<Datagram> datagram = m_socket.receive();
        if (!datagram.has_value()) {
            break;
        }

        const std::optional<ActorSnapshot> snapshot =
            decodeActorSnapshot(std::span<const std::byte>{datagram->bytes.data(), datagram->size});
        if (snapshot.has_value() && snapshot->clientId != m_clientId) {
            snapshots.push_back(*snapshot);
        }
    }
    return snapshots;
}
} // namespace vac::net
