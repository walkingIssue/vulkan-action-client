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
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

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

struct ClientRecord
{
    vac::net::Endpoint endpoint;
    uint8_t clientId = 0;
};

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

std::vector<ClientRecord>::iterator findClient(std::vector<ClientRecord> &clients,
                                               const vac::net::Endpoint &endpoint)
{
    return std::find_if(clients.begin(), clients.end(), [&](const ClientRecord &client) {
        return vac::net::sameEndpoint(client.endpoint, endpoint);
    });
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
        std::vector<ClientRecord> clients;
        uint32_t packetsReceived = 0;
        uint32_t packetsDumped = 0;

        while (options.maxPackets == 0 || packetsReceived < options.maxPackets) {
            std::optional<vac::net::Datagram> datagram = socket.receive();
            if (!datagram.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const std::optional<vac::net::ActorSnapshot> snapshot =
                vac::net::decodeActorSnapshot(std::span<const std::byte>{datagram->bytes.data(), datagram->size});
            if (!snapshot.has_value() || snapshot->clientId == 0) {
                continue;
            }

            ++packetsReceived;
            auto clientIt = findClient(clients, datagram->sender);
            if (clientIt == clients.end()) {
                clients.push_back({datagram->sender, snapshot->clientId});
                spdlog::info("Registered client {} at {}",
                             snapshot->clientId,
                             vac::net::endpointToString(datagram->sender));
            } else {
                clientIt->clientId = snapshot->clientId;
            }

            if (packetsDumped < options.dumpPackets) {
                ++packetsDumped;
                spdlog::info("snapshot client={} tick={} pos=({:.2f},{:.2f},{:.2f}) yaw={:.1f} flags=0x{:02x}",
                             snapshot->clientId,
                             snapshot->tick,
                             snapshot->position.x,
                             snapshot->position.y,
                             snapshot->position.z,
                             snapshot->yawDegrees,
                             snapshot->flags);
            }

            for (const ClientRecord &client : clients) {
                if (vac::net::sameEndpoint(client.endpoint, datagram->sender)) {
                    continue;
                }
                socket.sendTo(std::span<const std::byte>{datagram->bytes.data(), datagram->size}, client.endpoint);
            }
        }

        spdlog::info("UDP state relay exiting after {} received packets", packetsReceived);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "udp_state_server failed: " << error.what() << '\n';
        return 1;
    }
}
