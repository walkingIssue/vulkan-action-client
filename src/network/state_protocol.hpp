#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace vac::net
{
inline constexpr uint32_t kProtocolMagic = 0x31564341u; // "ACV1" in little-endian bit order.
inline constexpr uint8_t kProtocolVersion = 1;
inline constexpr size_t kMaxDatagramBytes = 256;

enum class PacketKind : uint8_t
{
    connect = 1,
    disconnect = 2,
    actorSnapshot = 3,
    serverEvent = 4,
};

enum class ServerEventKind : uint8_t
{
    clientConnected = 1,
    clientDisconnected = 2,
};

enum SnapshotFlags : uint8_t
{
    snapshotCameraSteering = 1u << 0u,
    snapshotSprinting = 1u << 1u,
    snapshotMoving = 1u << 2u,
};

struct ActorSnapshot
{
    uint8_t clientId = 0;
    uint16_t tick = 0;
    uint8_t flags = 0;
    glm::vec3 position{0.0f};
    float yawDegrees = 0.0f;
};

struct ConnectPacket
{
    uint8_t clientId = 0;
    uint16_t sequence = 0;
};

struct DisconnectPacket
{
    uint8_t clientId = 0;
    uint16_t sequence = 0;
};

struct ServerEventPacket
{
    ServerEventKind event = ServerEventKind::clientConnected;
    uint8_t clientId = 0;
    uint16_t sequence = 0;
};

using Packet = std::variant<ConnectPacket, DisconnectPacket, ActorSnapshot, ServerEventPacket>;

std::vector<std::byte> encodeConnectPacket(const ConnectPacket &packet);
std::vector<std::byte> encodeDisconnectPacket(const DisconnectPacket &packet);
std::vector<std::byte> encodeActorSnapshot(const ActorSnapshot &snapshot);
std::vector<std::byte> encodeServerEventPacket(const ServerEventPacket &packet);
std::optional<Packet> decodePacket(std::span<const std::byte> packet);
std::optional<ActorSnapshot> decodeActorSnapshot(std::span<const std::byte> packet);
} // namespace vac::net
