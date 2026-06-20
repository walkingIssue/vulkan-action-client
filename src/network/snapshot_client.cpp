#include "network/snapshot_client.hpp"

#include <span>
#include <stdexcept>

namespace vac::net
{
SnapshotClient::SnapshotClient(uint8_t clientId, Endpoint serverEndpoint)
    : m_clientId(clientId),
      m_serverEndpoint(serverEndpoint)
{
    if (m_clientId == 0) {
        throw std::invalid_argument("SnapshotClient requires a nonzero client id");
    }

    m_socket.open("0.0.0.0", 0, true);
    sendConnect();
}

SnapshotClient::~SnapshotClient()
{
    try {
        disconnect();
    } catch (...) {
    }
}

void SnapshotClient::disconnect()
{
    if (!m_connected) {
        return;
    }

    const std::vector<std::byte> packet = encodeDisconnectPacket({m_clientId, nextSequence()});
    m_socket.sendTo(packet, m_serverEndpoint);
    m_connected = false;
}

void SnapshotClient::sendSnapshot(ActorSnapshot snapshot)
{
    if (!m_connected) {
        return;
    }

    snapshot.clientId = m_clientId;
    const std::vector<std::byte> packet = encodeActorSnapshot(snapshot);
    m_socket.sendTo(packet, m_serverEndpoint);
}

SnapshotClient::ReceiveBatch SnapshotClient::receive()
{
    ReceiveBatch batch;
    while (true) {
        std::optional<Datagram> datagram = m_socket.receive();
        if (!datagram.has_value()) {
            break;
        }

        const std::optional<Packet> packet =
            decodePacket(std::span<const std::byte>{datagram->bytes.data(), datagram->size});
        if (!packet.has_value()) {
            continue;
        }

        if (const auto *snapshot = std::get_if<ActorSnapshot>(&*packet)) {
            if (snapshot->clientId != m_clientId) {
                batch.snapshots.push_back(*snapshot);
            }
        } else if (const auto *event = std::get_if<ServerEventPacket>(&*packet)) {
            batch.events.push_back(*event);
        }
    }
    return batch;
}

uint16_t SnapshotClient::nextSequence()
{
    return m_sequence++;
}

void SnapshotClient::sendConnect()
{
    const std::vector<std::byte> packet = encodeConnectPacket({m_clientId, nextSequence()});
    m_socket.sendTo(packet, m_serverEndpoint);
    m_connected = true;
}
} // namespace vac::net
