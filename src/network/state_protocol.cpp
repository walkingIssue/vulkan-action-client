#include "network/state_protocol.hpp"

#include <algorithm>
#include <cmath>

namespace vac::net
{
namespace
{
constexpr int kPositionBits = 22;
constexpr float kPositionScale = 100.0f;
constexpr uint32_t kYawQuantization = 65535u;

class BitWriter
{
public:
    void writeBits(uint32_t value, int bitCount)
    {
        for (int i = 0; i < bitCount; ++i) {
            if (m_bitOffset % 8 == 0) {
                m_bytes.push_back(std::byte{0});
            }

            if (((value >> i) & 1u) != 0u) {
                m_bytes.back() |= static_cast<std::byte>(1u << (m_bitOffset % 8));
            }
            ++m_bitOffset;
        }
    }

    void writeSigned(int32_t value, int bitCount)
    {
        const uint32_t mask = (1u << bitCount) - 1u;
        writeBits(static_cast<uint32_t>(value) & mask, bitCount);
    }

    std::vector<std::byte> finish() &&
    {
        return std::move(m_bytes);
    }

private:
    std::vector<std::byte> m_bytes;
    size_t m_bitOffset = 0;
};

class BitReader
{
public:
    explicit BitReader(std::span<const std::byte> bytes)
        : m_bytes(bytes)
    {
    }

    std::optional<uint32_t> readBits(int bitCount)
    {
        if (m_bitOffset + static_cast<size_t>(bitCount) > m_bytes.size() * 8) {
            return std::nullopt;
        }

        uint32_t value = 0;
        for (int i = 0; i < bitCount; ++i) {
            const uint8_t byte = static_cast<uint8_t>(m_bytes[m_bitOffset / 8]);
            const uint8_t bit = static_cast<uint8_t>((byte >> (m_bitOffset % 8)) & 1u);
            value |= static_cast<uint32_t>(bit) << i;
            ++m_bitOffset;
        }
        return value;
    }

    std::optional<int32_t> readSigned(int bitCount)
    {
        const std::optional<uint32_t> raw = readBits(bitCount);
        if (!raw.has_value()) {
            return std::nullopt;
        }

        const uint32_t signBit = 1u << (bitCount - 1);
        if ((*raw & signBit) == 0u) {
            return static_cast<int32_t>(*raw);
        }

        const uint32_t mask = (1u << bitCount) - 1u;
        return static_cast<int32_t>(*raw | ~mask);
    }

private:
    std::span<const std::byte> m_bytes;
    size_t m_bitOffset = 0;
};

int32_t quantizePosition(float value)
{
    constexpr int32_t minValue = -(1 << (kPositionBits - 1));
    constexpr int32_t maxValue = (1 << (kPositionBits - 1)) - 1;
    return std::clamp(static_cast<int32_t>(std::lround(value * kPositionScale)), minValue, maxValue);
}

float dequantizePosition(int32_t value)
{
    return static_cast<float>(value) / kPositionScale;
}

uint32_t quantizeYaw(float yawDegrees)
{
    float wrapped = std::fmod(yawDegrees, 360.0f);
    if (wrapped < 0.0f) {
        wrapped += 360.0f;
    }
    return std::clamp(static_cast<uint32_t>(std::lround((wrapped / 360.0f) * kYawQuantization)),
                      0u,
                      kYawQuantization);
}

float dequantizeYaw(uint32_t value)
{
    return (static_cast<float>(value) / static_cast<float>(kYawQuantization)) * 360.0f;
}

void writeHeader(BitWriter &writer, PacketKind kind)
{
    writer.writeBits(kProtocolMagic, 32);
    writer.writeBits(kProtocolVersion, 4);
    writer.writeBits(static_cast<uint8_t>(kind), 4);
}

std::optional<PacketKind> readHeader(BitReader &reader)
{
    const std::optional<uint32_t> magic = reader.readBits(32);
    const std::optional<uint32_t> version = reader.readBits(4);
    const std::optional<uint32_t> kind = reader.readBits(4);
    if (!magic.has_value() ||
        !version.has_value() ||
        !kind.has_value() ||
        *magic != kProtocolMagic ||
        *version != kProtocolVersion) {
        return std::nullopt;
    }

    switch (static_cast<PacketKind>(*kind)) {
    case PacketKind::connect:
    case PacketKind::disconnect:
    case PacketKind::actorSnapshot:
    case PacketKind::serverEvent:
        return static_cast<PacketKind>(*kind);
    }

    return std::nullopt;
}

std::optional<ConnectPacket> readConnectPacket(BitReader &reader)
{
    const std::optional<uint32_t> clientId = reader.readBits(8);
    const std::optional<uint32_t> sequence = reader.readBits(16);
    if (!clientId.has_value() || !sequence.has_value()) {
        return std::nullopt;
    }

    return ConnectPacket{static_cast<uint8_t>(*clientId), static_cast<uint16_t>(*sequence)};
}

std::optional<DisconnectPacket> readDisconnectPacket(BitReader &reader)
{
    const std::optional<uint32_t> clientId = reader.readBits(8);
    const std::optional<uint32_t> sequence = reader.readBits(16);
    if (!clientId.has_value() || !sequence.has_value()) {
        return std::nullopt;
    }

    return DisconnectPacket{static_cast<uint8_t>(*clientId), static_cast<uint16_t>(*sequence)};
}

std::optional<ActorSnapshot> readActorSnapshot(BitReader &reader)
{
    const std::optional<uint32_t> clientId = reader.readBits(8);
    const std::optional<uint32_t> tick = reader.readBits(16);
    const std::optional<uint32_t> flags = reader.readBits(8);
    const std::optional<int32_t> x = reader.readSigned(kPositionBits);
    const std::optional<int32_t> y = reader.readSigned(kPositionBits);
    const std::optional<int32_t> z = reader.readSigned(kPositionBits);
    const std::optional<uint32_t> yaw = reader.readBits(16);
    if (!clientId.has_value() ||
        !tick.has_value() ||
        !flags.has_value() ||
        !x.has_value() ||
        !y.has_value() ||
        !z.has_value() ||
        !yaw.has_value()) {
        return std::nullopt;
    }

    return ActorSnapshot{
        static_cast<uint8_t>(*clientId),
        static_cast<uint16_t>(*tick),
        static_cast<uint8_t>(*flags),
        {dequantizePosition(*x), dequantizePosition(*y), dequantizePosition(*z)},
        dequantizeYaw(*yaw),
    };
}

std::optional<ServerEventPacket> readServerEventPacket(BitReader &reader)
{
    const std::optional<uint32_t> event = reader.readBits(8);
    const std::optional<uint32_t> clientId = reader.readBits(8);
    const std::optional<uint32_t> sequence = reader.readBits(16);
    if (!event.has_value() || !clientId.has_value() || !sequence.has_value()) {
        return std::nullopt;
    }

    const auto kind = static_cast<ServerEventKind>(*event);
    if (kind != ServerEventKind::clientConnected && kind != ServerEventKind::clientDisconnected) {
        return std::nullopt;
    }

    return ServerEventPacket{kind, static_cast<uint8_t>(*clientId), static_cast<uint16_t>(*sequence)};
}
} // namespace

std::vector<std::byte> encodeConnectPacket(const ConnectPacket &packet)
{
    BitWriter writer;
    writeHeader(writer, PacketKind::connect);
    writer.writeBits(packet.clientId, 8);
    writer.writeBits(packet.sequence, 16);
    return std::move(writer).finish();
}

std::vector<std::byte> encodeDisconnectPacket(const DisconnectPacket &packet)
{
    BitWriter writer;
    writeHeader(writer, PacketKind::disconnect);
    writer.writeBits(packet.clientId, 8);
    writer.writeBits(packet.sequence, 16);
    return std::move(writer).finish();
}

std::vector<std::byte> encodeActorSnapshot(const ActorSnapshot &snapshot)
{
    BitWriter writer;
    writeHeader(writer, PacketKind::actorSnapshot);
    writer.writeBits(snapshot.clientId, 8);
    writer.writeBits(snapshot.tick, 16);
    writer.writeBits(snapshot.flags, 8);
    writer.writeSigned(quantizePosition(snapshot.position.x), kPositionBits);
    writer.writeSigned(quantizePosition(snapshot.position.y), kPositionBits);
    writer.writeSigned(quantizePosition(snapshot.position.z), kPositionBits);
    writer.writeBits(quantizeYaw(snapshot.yawDegrees), 16);
    return std::move(writer).finish();
}

std::vector<std::byte> encodeServerEventPacket(const ServerEventPacket &packet)
{
    BitWriter writer;
    writeHeader(writer, PacketKind::serverEvent);
    writer.writeBits(static_cast<uint8_t>(packet.event), 8);
    writer.writeBits(packet.clientId, 8);
    writer.writeBits(packet.sequence, 16);
    return std::move(writer).finish();
}

std::optional<Packet> decodePacket(std::span<const std::byte> packet)
{
    BitReader reader(packet);
    const std::optional<PacketKind> kind = readHeader(reader);
    if (!kind.has_value()) {
        return std::nullopt;
    }

    switch (*kind) {
    case PacketKind::connect:
        if (const std::optional<ConnectPacket> connect = readConnectPacket(reader)) {
            return *connect;
        }
        break;
    case PacketKind::disconnect:
        if (const std::optional<DisconnectPacket> disconnect = readDisconnectPacket(reader)) {
            return *disconnect;
        }
        break;
    case PacketKind::actorSnapshot:
        if (const std::optional<ActorSnapshot> snapshot = readActorSnapshot(reader)) {
            return *snapshot;
        }
        break;
    case PacketKind::serverEvent:
        if (const std::optional<ServerEventPacket> event = readServerEventPacket(reader)) {
            return *event;
        }
        break;
    }

    return std::nullopt;
}

std::optional<ActorSnapshot> decodeActorSnapshot(std::span<const std::byte> packet)
{
    const std::optional<Packet> decoded = decodePacket(packet);
    if (!decoded.has_value()) {
        return std::nullopt;
    }

    if (const auto *snapshot = std::get_if<ActorSnapshot>(&*decoded)) {
        return *snapshot;
    }

    return std::nullopt;
}
} // namespace vac::net
