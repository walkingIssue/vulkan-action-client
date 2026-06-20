#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#include <glm/glm.hpp>

#include "network/snapshot_client.hpp"
#include "network/state_protocol.hpp"
#include "network/udp_socket.hpp"

namespace
{
struct ClientOptions
{
    std::string server = "127.0.0.1:40000";
    uint8_t clientId = 1;
    int snapshots = 96;
    int expectRemoteSnapshots = 1;
    int remoteTimeoutMs = 3000;
};

ClientOptions parseOptions(int argc, char **argv)
{
    ClientOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--server" && i + 1 < argc) {
            options.server = argv[++i];
        } else if (arg == "--client-id" && i + 1 < argc) {
            const int clientId = std::stoi(argv[++i]);
            if (clientId <= 0 || clientId > 255) {
                throw std::runtime_error("--client-id must be between 1 and 255");
            }
            options.clientId = static_cast<uint8_t>(clientId);
        } else if (arg == "--snapshots" && i + 1 < argc) {
            options.snapshots = std::max(0, std::stoi(argv[++i]));
        } else if (arg == "--expect-remote-snapshots" && i + 1 < argc) {
            options.expectRemoteSnapshots = std::max(0, std::stoi(argv[++i]));
        } else if (arg == "--remote-timeout-ms" && i + 1 < argc) {
            options.remoteTimeoutMs = std::max(1, std::stoi(argv[++i]));
        }
    }
    return options;
}
} // namespace

int main(int argc, char **argv)
{
    try {
        const ClientOptions options = parseOptions(argc, argv);
        vac::net::SnapshotClient client(options.clientId, vac::net::resolveEndpoint(options.server));

        int remoteSnapshots = 0;
        int peerEvents = 0;
        for (int i = 0; i < options.snapshots; ++i) {
            client.sendSnapshot({
                options.clientId,
                static_cast<uint16_t>(i),
                vac::net::snapshotMoving,
                glm::vec3{static_cast<float>(options.clientId) * 10.0f, 0.0f, static_cast<float>(i) * 0.1f},
                static_cast<float>((i * 7) % 360),
            });

            const vac::net::SnapshotClient::ReceiveBatch batch = client.receive();
            remoteSnapshots += static_cast<int>(batch.snapshots.size());
            peerEvents += static_cast<int>(batch.events.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(options.remoteTimeoutMs);
        while (remoteSnapshots < options.expectRemoteSnapshots && std::chrono::steady_clock::now() < deadline) {
            const vac::net::SnapshotClient::ReceiveBatch batch = client.receive();
            remoteSnapshots += static_cast<int>(batch.snapshots.size());
            peerEvents += static_cast<int>(batch.events.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client.disconnect();

        std::cout << "client=" << static_cast<int>(options.clientId)
                  << " remoteSnapshots=" << remoteSnapshots
                  << " peerEvents=" << peerEvents << '\n';

        if (remoteSnapshots < options.expectRemoteSnapshots) {
            std::cerr << "Expected at least " << options.expectRemoteSnapshots
                      << " remote snapshots, received " << remoteSnapshots << '\n';
            return 2;
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "network_e2e_client failed: " << error.what() << '\n';
        return 1;
    }
}
