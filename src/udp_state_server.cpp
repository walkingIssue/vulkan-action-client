#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "network/snapshot_relay.hpp"
#include "network/state_protocol.hpp"
#include "network/udp_socket.hpp"

namespace
{
struct ServerOptions
{
    std::string bind = "127.0.0.1:40000";
    uint32_t maxPackets = 0;
    uint32_t dumpPackets = 128;
};

template <typename... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

std::pair<std::string, uint16_t> splitHostPort(const std::string &hostAndPort)
{
    const size_t separator = hostAndPort.rfind(':');
    if (separator == std::string::npos) {
        throw std::runtime_error(fmt::format("Endpoint '{}' must be host:port", hostAndPort));
    }

    const std::string host = hostAndPort.substr(0, separator);
    const int port = std::stoi(hostAndPort.substr(separator + 1));
    if (port <= 0 || port > 65535) {
        throw std::runtime_error(fmt::format("Endpoint '{}' has invalid port", hostAndPort));
    }
    return {host.empty() ? "0.0.0.0" : host, static_cast<uint16_t>(port)};
}

ServerOptions parseOptions(int argc, char **argv)
{
    ServerOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--bind" && i + 1 < argc) {
            options.bind = argv[++i];
        } else if (arg == "--max-packets" && i + 1 < argc) {
            options.maxPackets = static_cast<uint32_t>(std::max(0, std::stoi(argv[++i])));
        } else if (arg == "--dump-packets" && i + 1 < argc) {
            options.dumpPackets = static_cast<uint32_t>(std::max(0, std::stoi(argv[++i])));
        }
    }
    return options;
}

void logAcceptedPacket(const vac::net::Packet &packet,
                       const std::string &senderKey,
                       uint32_t dumpPackets,
                       uint32_t &packetsDumped)
{
    std::visit(Overloaded{
                   [&](const vac::net::ConnectPacket &connect) {
                       spdlog::info("client {} connected from {}", connect.clientId, senderKey);
                   },
                   [&](const vac::net::DisconnectPacket &disconnect) {
                       spdlog::info("client {} disconnected from {}", disconnect.clientId, senderKey);
                   },
                   [&](const vac::net::ActorSnapshot &snapshot) {
                       if (packetsDumped >= dumpPackets) {
                           return;
                       }

                       ++packetsDumped;
                       spdlog::info("snapshot client={} tick={} pos=({:.2f},{:.2f},{:.2f}) yaw={:.1f} flags=0x{:02x}",
                                    snapshot.clientId,
                                    snapshot.tick,
                                    snapshot.position.x,
                                    snapshot.position.y,
                                    snapshot.position.z,
                                    snapshot.yawDegrees,
                                    snapshot.flags);
                   },
                   [](const vac::net::ServerEventPacket &) {
                   },
               },
               packet);
}
} // namespace

int main(int argc, char **argv)
{
    try {
        const ServerOptions options = parseOptions(argc, argv);
        const auto [bindHost, bindPort] = splitHostPort(options.bind);

        vac::net::UdpSocket socket;
        socket.open(bindHost, bindPort, false);

        spdlog::info("UDP state relay listening on {}", options.bind);

        vac::net::SnapshotRelay relay;
        std::unordered_map<std::string, vac::net::Endpoint> endpointsByKey;
        uint32_t packetsAccepted = 0;
        uint32_t packetsDumped = 0;

        while (options.maxPackets == 0 || packetsAccepted < options.maxPackets) {
            std::optional<vac::net::Datagram> datagram = socket.receive();
            if (!datagram.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const std::string senderKey = vac::net::endpointToString(datagram->sender);
            endpointsByKey[senderKey] = datagram->sender;

            const vac::net::RelayResult result = relay.ingest(
                senderKey,
                std::span<const std::byte>{datagram->bytes.data(), datagram->size});
            if (!result.packet.has_value()) {
                continue;
            }

            if (!result.accepted) {
                spdlog::warn("Rejected packet from {}", senderKey);
                continue;
            }

            ++packetsAccepted;
            logAcceptedPacket(*result.packet, senderKey, options.dumpPackets, packetsDumped);

            for (const vac::net::RelayOutput &output : result.outputs) {
                const auto endpointIt = endpointsByKey.find(output.endpointKey);
                if (endpointIt == endpointsByKey.end()) {
                    continue;
                }
                socket.sendTo(output.packet, endpointIt->second);
            }
        }

        spdlog::info("UDP state relay exiting after {} accepted packets", packetsAccepted);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "udp_state_server failed: " << error.what() << '\n';
        return 1;
    }
}
