#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "network/state_protocol.hpp"

namespace vac::net
{
struct RelayOutput
{
    std::string endpointKey;
    std::vector<std::byte> packet;
};

struct RelayResult
{
    bool accepted = false;
    std::optional<Packet> packet;
    std::vector<RelayOutput> outputs;
};

class SnapshotRelay
{
public:
    RelayResult ingest(const std::string &endpointKey, std::span<const std::byte> packet);

    size_t clientCount() const { return m_clientsById.size(); }
    bool hasClient(uint8_t clientId) const;
    std::optional<std::string> endpointForClient(uint8_t clientId) const;

private:
    struct ClientSession
    {
        uint8_t clientId = 0;
        std::string endpointKey;
        uint16_t lastSequence = 0;
    };

    bool endpointOwnsClient(const std::string &endpointKey, uint8_t clientId) const;
    void removeEndpoint(const std::string &endpointKey);
    void removeClient(uint8_t clientId);

    std::unordered_map<uint8_t, ClientSession> m_clientsById;
    std::unordered_map<std::string, uint8_t> m_clientIdByEndpoint;
};
} // namespace vac::net
