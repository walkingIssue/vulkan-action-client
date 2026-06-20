#pragma once

#include <vector>

#include "network/state_protocol.hpp"
#include "network/udp_socket.hpp"

namespace vac::net
{
class SnapshotClient
{
public:
    SnapshotClient(uint8_t clientId, Endpoint serverEndpoint);
    ~SnapshotClient();

    uint8_t clientId() const { return m_clientId; }
    void disconnect();
    void sendSnapshot(ActorSnapshot snapshot);

    struct ReceiveBatch
    {
        std::vector<ActorSnapshot> snapshots;
        std::vector<ServerEventPacket> events;
    };

    ReceiveBatch receive();

private:
    uint16_t nextSequence();
    void sendConnect();

    uint8_t m_clientId = 0;
    uint16_t m_sequence = 1;
    bool m_connected = false;
    Endpoint m_serverEndpoint;
    UdpSocket m_socket;
};
} // namespace vac::net
