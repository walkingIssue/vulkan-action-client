#include "network/snapshot_relay.hpp"

namespace vac::net
{
namespace
{
std::vector<std::byte> copyPacket(std::span<const std::byte> packet)
{
    return {packet.begin(), packet.end()};
}

void fanOutServerEvent(std::vector<RelayOutput> &outputs,
                       const std::string &endpointKey,
                       ServerEventKind event,
                       uint8_t clientId,
                       uint16_t sequence)
{
    outputs.push_back({
        endpointKey,
        encodeServerEventPacket({event, clientId, sequence}),
    });
}
} // namespace

RelayResult SnapshotRelay::ingest(const std::string &endpointKey, std::span<const std::byte> packet)
{
    RelayResult result;
    result.packet = decodePacket(packet);
    if (!result.packet.has_value()) {
        return result;
    }

    if (const auto *connect = std::get_if<ConnectPacket>(&*result.packet)) {
        if (connect->clientId == 0) {
            return result;
        }

        const bool duplicateConnect = endpointOwnsClient(endpointKey, connect->clientId);
        if (duplicateConnect) {
            m_clientsById[connect->clientId].lastSequence = connect->sequence;
            result.accepted = true;
            return result;
        }

        std::vector<ClientSession> existingClients;
        existingClients.reserve(m_clientsById.size());
        for (const auto &[clientId, session] : m_clientsById) {
            if (clientId != connect->clientId) {
                existingClients.push_back(session);
            }
        }

        removeEndpoint(endpointKey);
        removeClient(connect->clientId);

        m_clientIdByEndpoint[endpointKey] = connect->clientId;
        m_clientsById[connect->clientId] = {connect->clientId, endpointKey, connect->sequence};

        for (const ClientSession &peer : existingClients) {
            fanOutServerEvent(result.outputs,
                              endpointKey,
                              ServerEventKind::clientConnected,
                              peer.clientId,
                              peer.lastSequence);
            fanOutServerEvent(result.outputs,
                              peer.endpointKey,
                              ServerEventKind::clientConnected,
                              connect->clientId,
                              connect->sequence);
        }

        result.accepted = true;
        return result;
    }

    if (const auto *disconnect = std::get_if<DisconnectPacket>(&*result.packet)) {
        if (disconnect->clientId == 0 || !endpointOwnsClient(endpointKey, disconnect->clientId)) {
            return result;
        }

        removeClient(disconnect->clientId);
        for (const auto &[clientId, session] : m_clientsById) {
            (void)clientId;
            fanOutServerEvent(result.outputs,
                              session.endpointKey,
                              ServerEventKind::clientDisconnected,
                              disconnect->clientId,
                              disconnect->sequence);
        }

        result.accepted = true;
        return result;
    }

    if (const auto *snapshot = std::get_if<ActorSnapshot>(&*result.packet)) {
        if (snapshot->clientId == 0 || !endpointOwnsClient(endpointKey, snapshot->clientId)) {
            return result;
        }

        m_clientsById[snapshot->clientId].lastSequence = snapshot->tick;
        for (const auto &[clientId, session] : m_clientsById) {
            if (clientId == snapshot->clientId) {
                continue;
            }
            result.outputs.push_back({session.endpointKey, copyPacket(packet)});
        }

        result.accepted = true;
        return result;
    }

    return result;
}

bool SnapshotRelay::hasClient(uint8_t clientId) const
{
    return m_clientsById.find(clientId) != m_clientsById.end();
}

std::optional<std::string> SnapshotRelay::endpointForClient(uint8_t clientId) const
{
    const auto clientIt = m_clientsById.find(clientId);
    if (clientIt == m_clientsById.end()) {
        return std::nullopt;
    }
    return clientIt->second.endpointKey;
}

bool SnapshotRelay::endpointOwnsClient(const std::string &endpointKey, uint8_t clientId) const
{
    const auto endpointIt = m_clientIdByEndpoint.find(endpointKey);
    return endpointIt != m_clientIdByEndpoint.end() && endpointIt->second == clientId;
}

void SnapshotRelay::removeEndpoint(const std::string &endpointKey)
{
    const auto endpointIt = m_clientIdByEndpoint.find(endpointKey);
    if (endpointIt == m_clientIdByEndpoint.end()) {
        return;
    }

    const uint8_t clientId = endpointIt->second;
    m_clientIdByEndpoint.erase(endpointIt);

    const auto clientIt = m_clientsById.find(clientId);
    if (clientIt != m_clientsById.end() && clientIt->second.endpointKey == endpointKey) {
        m_clientsById.erase(clientIt);
    }
}

void SnapshotRelay::removeClient(uint8_t clientId)
{
    const auto clientIt = m_clientsById.find(clientId);
    if (clientIt == m_clientsById.end()) {
        return;
    }

    m_clientIdByEndpoint.erase(clientIt->second.endpointKey);
    m_clientsById.erase(clientIt);
}
} // namespace vac::net
