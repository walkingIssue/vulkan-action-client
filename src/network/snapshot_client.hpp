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

    uint8_t clientId() const { return m_clientId; }
    void sendSnapshot(ActorSnapshot snapshot);
    std::vector<ActorSnapshot> receiveSnapshots();

private:
    uint8_t m_clientId = 0;
    Endpoint m_serverEndpoint;
    UdpSocket m_socket;
};
} // namespace vac::net
