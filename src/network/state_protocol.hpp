#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace vac::net
{
inline constexpr uint32_t kProtocolMagic = 0x31564341u; // "ACV1" in little-endian bit order.
inline constexpr uint8_t kProtocolVersion = 1;
inline constexpr size_t kMaxDatagramBytes = 256;

enum class PacketKind : uint8_t
{
    actorSnapshot = 1,
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

std::vector<std::byte> encodeActorSnapshot(const ActorSnapshot &snapshot);
std::optional<ActorSnapshot> decodeActorSnapshot(std::span<const std::byte> packet);
} // namespace vac::net
