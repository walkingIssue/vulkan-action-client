#include "host/host_cli.hpp"
#include "network/snapshot_client.hpp"
#include "network/snapshot_relay.hpp"
#include "network/state_protocol.hpp"
#include "network/udp_socket.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace {
using Clock = std::chrono::steady_clock;

struct Diagnostic {
  std::string severity = "error";
  std::string code;
  std::string message;
};

struct RunnerOptions {
  std::string bindHost = "127.0.0.1";
  uint16_t port = 45621;
  uint32_t clientCount = 2;
  uint32_t snapshotsPerClient = 96;
  uint32_t minRemoteSnapshots = 1;
  uint32_t remoteTimeoutMs = 3000;
  uint32_t drainStepMs = 2;
};

struct RelayStats {
  uint32_t datagramsReceived = 0;
  uint32_t invalidDatagrams = 0;
  uint32_t rejectedPackets = 0;
  uint32_t acceptedConnects = 0;
  uint32_t acceptedDisconnects = 0;
  uint32_t acceptedSnapshots = 0;
  uint32_t outputsSent = 0;
  uint32_t connectEventsSent = 0;
  uint32_t disconnectEventsSent = 0;
  uint32_t serverErrors = 0;
  size_t finalClientCount = 0;
  std::vector<Diagnostic> diagnostics;
};

struct ClientSlot {
  uint8_t clientId = 0;
  std::unique_ptr<vac::net::SnapshotClient> client;
  uint32_t remoteSnapshots = 0;
  uint32_t connectEvents = 0;
  uint32_t disconnectEvents = 0;
};

uint32_t parseUint32Option(std::string_view name, const std::string &value) {
  try {
    size_t consumed = 0;
    const unsigned long parsed = std::stoul(value, &consumed, 10);
    if (consumed == value.size() &&
        parsed <= std::numeric_limits<uint32_t>::max()) {
      return static_cast<uint32_t>(parsed);
    }
  } catch (const std::exception &) {
  }

  throw vac::host::ParseError(
      fmt::format("{} requires a uint32 value, got '{}'", name, value));
}

uint16_t parsePortOption(std::string_view name, const std::string &value) {
  const uint32_t parsed = parseUint32Option(name, value);
  if (parsed == 0 || parsed > 65535) {
    throw vac::host::ParseError(
        fmt::format("{} must be between 1 and 65535", name));
  }
  return static_cast<uint16_t>(parsed);
}

RunnerOptions parseRunnerOptions(vac::host::CommandLine &commandLine) {
  RunnerOptions options;
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--bind-host")) {
    options.bindHost = *value;
    if (options.bindHost.empty()) {
      throw vac::host::ParseError("--bind-host cannot be empty");
    }
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--port")) {
    options.port = parsePortOption("--port", *value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--client-count")) {
    options.clientCount = parseUint32Option("--client-count", *value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--snapshots")) {
    options.snapshotsPerClient = parseUint32Option("--snapshots", *value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--min-remote-snapshots")) {
    options.minRemoteSnapshots =
        parseUint32Option("--min-remote-snapshots", *value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--remote-timeout-ms")) {
    options.remoteTimeoutMs = parseUint32Option("--remote-timeout-ms", *value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--drain-step-ms")) {
    options.drainStepMs = parseUint32Option("--drain-step-ms", *value);
  }

  if (options.clientCount < 2 || options.clientCount > 255) {
    throw vac::host::ParseError("--client-count must be between 2 and 255");
  }
  if (options.snapshotsPerClient == 0) {
    throw vac::host::ParseError("--snapshots must be greater than zero");
  }
  if (options.minRemoteSnapshots == 0) {
    throw vac::host::ParseError(
        "--min-remote-snapshots must be greater than zero");
  }
  if (options.remoteTimeoutMs < 10) {
    throw vac::host::ParseError("--remote-timeout-ms must be at least 10");
  }
  if (options.drainStepMs == 0 || options.drainStepMs > 100) {
    throw vac::host::ParseError("--drain-step-ms must be between 1 and 100");
  }

  return options;
}

template <typename... Visitors> struct Overloaded : Visitors... {
  using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

nlohmann::json diagnosticToJson(const Diagnostic &diagnostic) {
  return {
      {"severity", diagnostic.severity},
      {"code", diagnostic.code},
      {"message", diagnostic.message},
  };
}

nlohmann::json relayStatsToJson(const RelayStats &stats) {
  nlohmann::json diagnostics = nlohmann::json::array();
  for (const Diagnostic &diagnostic : stats.diagnostics) {
    diagnostics.push_back(diagnosticToJson(diagnostic));
  }

  return {
      {"datagramsReceived", stats.datagramsReceived},
      {"invalidDatagrams", stats.invalidDatagrams},
      {"rejectedPackets", stats.rejectedPackets},
      {"acceptedConnects", stats.acceptedConnects},
      {"acceptedDisconnects", stats.acceptedDisconnects},
      {"acceptedSnapshots", stats.acceptedSnapshots},
      {"outputsSent", stats.outputsSent},
      {"connectEventsSent", stats.connectEventsSent},
      {"disconnectEventsSent", stats.disconnectEventsSent},
      {"serverErrors", stats.serverErrors},
      {"finalClientCount", stats.finalClientCount},
      {"diagnostics", std::move(diagnostics)},
  };
}

nlohmann::json clientSlotToJson(const ClientSlot &slot) {
  return {
      {"clientId", slot.clientId},
      {"remoteSnapshots", slot.remoteSnapshots},
      {"connectEvents", slot.connectEvents},
      {"disconnectEvents", slot.disconnectEvents},
  };
}

void writeJsonFile(const std::filesystem::path &path,
                   const nlohmann::json &document) {
  if (path.empty()) {
    return;
  }

  const std::filesystem::path absolutePath = std::filesystem::absolute(path);
  const std::filesystem::path parent = absolutePath.parent_path();
  std::error_code error;
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      throw std::runtime_error(
          fmt::format("Could not create result directory '{}': {}",
                      parent.string(), error.message()));
    }
  }

  const std::filesystem::path temporaryPath = absolutePath.string() + ".tmp";
  {
    std::ofstream output{temporaryPath, std::ios::trunc};
    if (!output) {
      throw std::runtime_error(fmt::format("Could not open result file '{}'",
                                           temporaryPath.string()));
    }
    output << document.dump(2) << '\n';
  }

  std::filesystem::remove(absolutePath, error);
  error.clear();
  std::filesystem::rename(temporaryPath, absolutePath, error);
  if (error) {
    throw std::runtime_error(
        fmt::format("Could not replace result file '{}': {}",
                    absolutePath.string(), error.message()));
  }
}

class RelayRuntime {
public:
  RelayRuntime(std::string bindHost, uint16_t port)
      : m_bindHost(std::move(bindHost)), m_port(port) {}

  ~RelayRuntime() { stop(); }

  RelayRuntime(const RelayRuntime &) = delete;
  RelayRuntime &operator=(const RelayRuntime &) = delete;

  void start() {
    m_socket.open(m_bindHost, m_port, true);
    m_thread = std::thread{[this]() { run(); }};
  }

  void stop() {
    m_stopping.store(true);
    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

  RelayStats stats() const {
    std::lock_guard lock{m_mutex};
    return m_stats;
  }

private:
  void run() {
    try {
      while (!m_stopping.load()) {
        const std::optional<vac::net::Datagram> datagram = m_socket.receive();
        if (!datagram.has_value()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }

        handleDatagram(*datagram);
      }
    } catch (const std::exception &error) {
      std::lock_guard lock{m_mutex};
      ++m_stats.serverErrors;
      m_stats.diagnostics.push_back(
          {"error", "relay_thread_error", error.what()});
    }
  }

  void handleDatagram(const vac::net::Datagram &datagram) {
    const std::string senderKey = vac::net::endpointToString(datagram.sender);
    m_endpointsByKey[senderKey] = datagram.sender;

    const vac::net::RelayResult result = m_relay.ingest(
        senderKey,
        std::span<const std::byte>{datagram.bytes.data(), datagram.size});

    RelayStats increment;
    increment.datagramsReceived = 1;
    if (!result.packet.has_value()) {
      increment.invalidDatagrams = 1;
    } else if (!result.accepted) {
      increment.rejectedPackets = 1;
    } else {
      std::visit(Overloaded{
                     [&](const vac::net::ConnectPacket &) {
                       increment.acceptedConnects = 1;
                     },
                     [&](const vac::net::DisconnectPacket &) {
                       increment.acceptedDisconnects = 1;
                     },
                     [&](const vac::net::ActorSnapshot &) {
                       increment.acceptedSnapshots = 1;
                     },
                     [&](const vac::net::ServerEventPacket &) {},
                 },
                 *result.packet);
    }

    for (const vac::net::RelayOutput &output : result.outputs) {
      const std::optional<vac::net::Packet> decoded =
          vac::net::decodePacket(output.packet);
      if (const auto *event =
              decoded ? std::get_if<vac::net::ServerEventPacket>(&*decoded)
                      : nullptr) {
        if (event->event == vac::net::ServerEventKind::clientConnected) {
          ++increment.connectEventsSent;
        } else if (event->event ==
                   vac::net::ServerEventKind::clientDisconnected) {
          ++increment.disconnectEventsSent;
        }
      }

      const auto endpointIt = m_endpointsByKey.find(output.endpointKey);
      if (endpointIt == m_endpointsByKey.end()) {
        continue;
      }
      m_socket.sendTo(output.packet, endpointIt->second);
      ++increment.outputsSent;
    }

    {
      std::lock_guard lock{m_mutex};
      m_stats.datagramsReceived += increment.datagramsReceived;
      m_stats.invalidDatagrams += increment.invalidDatagrams;
      m_stats.rejectedPackets += increment.rejectedPackets;
      m_stats.acceptedConnects += increment.acceptedConnects;
      m_stats.acceptedDisconnects += increment.acceptedDisconnects;
      m_stats.acceptedSnapshots += increment.acceptedSnapshots;
      m_stats.outputsSent += increment.outputsSent;
      m_stats.connectEventsSent += increment.connectEventsSent;
      m_stats.disconnectEventsSent += increment.disconnectEventsSent;
      m_stats.finalClientCount = m_relay.clientCount();
    }
  }

  std::string m_bindHost;
  uint16_t m_port = 0;
  vac::net::UdpSocket m_socket;
  vac::net::SnapshotRelay m_relay;
  std::unordered_map<std::string, vac::net::Endpoint> m_endpointsByKey;
  std::atomic_bool m_stopping = false;
  std::thread m_thread;
  mutable std::mutex m_mutex;
  RelayStats m_stats;
};

void drainClient(ClientSlot &slot) {
  if (slot.client == nullptr) {
    return;
  }

  const vac::net::SnapshotClient::ReceiveBatch batch = slot.client->receive();
  slot.remoteSnapshots += static_cast<uint32_t>(batch.snapshots.size());
  for (const vac::net::ServerEventPacket &event : batch.events) {
    if (event.event == vac::net::ServerEventKind::clientConnected) {
      ++slot.connectEvents;
    } else if (event.event == vac::net::ServerEventKind::clientDisconnected) {
      ++slot.disconnectEvents;
    }
  }
}

void drainClientsOnce(std::vector<ClientSlot> &clients) {
  for (ClientSlot &client : clients) {
    drainClient(client);
  }
}

void drainClientsFor(std::vector<ClientSlot> &clients,
                     std::chrono::milliseconds duration) {
  const Clock::time_point deadline = Clock::now() + duration;
  do {
    drainClientsOnce(clients);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (Clock::now() < deadline);
}

bool allClientsHaveRemoteSnapshots(const std::vector<ClientSlot> &clients,
                                   uint32_t minRemoteSnapshots) {
  return std::all_of(clients.begin(), clients.end(),
                     [minRemoteSnapshots](const ClientSlot &client) {
                       return client.remoteSnapshots >= minRemoteSnapshots;
                     });
}

uint32_t totalDisconnectEvents(const std::vector<ClientSlot> &clients) {
  uint32_t total = 0;
  for (const ClientSlot &client : clients) {
    total += client.disconnectEvents;
  }
  return total;
}

uint32_t totalConnectEvents(const std::vector<ClientSlot> &clients) {
  uint32_t total = 0;
  for (const ClientSlot &client : clients) {
    total += client.connectEvents;
  }
  return total;
}

vac::net::ActorSnapshot snapshotFor(uint8_t clientId, uint16_t tick) {
  return {
      clientId,
      tick,
      vac::net::snapshotMoving,
      {static_cast<float>(clientId) * 8.0f, 0.0f,
       static_cast<float>(tick) * 0.1f},
      static_cast<float>((static_cast<uint32_t>(clientId) * 31u + tick) % 360u),
  };
}

void appendFailureDiagnostics(const RunnerOptions &options,
                              const RelayStats &relayStats,
                              const std::vector<ClientSlot> &clients,
                              std::vector<Diagnostic> &diagnostics) {
  if (relayStats.serverErrors > 0) {
    diagnostics.push_back(
        {"error", "relay_thread_error", "The relay thread reported errors"});
  }
  if (relayStats.acceptedConnects < options.clientCount) {
    diagnostics.push_back(
        {"error", "missing_connects",
         fmt::format("Expected at least {} accepted connects, observed {}",
                     options.clientCount, relayStats.acceptedConnects)});
  }
  if (relayStats.acceptedDisconnects < options.clientCount) {
    diagnostics.push_back(
        {"error", "missing_disconnects",
         fmt::format("Expected at least {} accepted disconnects, observed {}",
                     options.clientCount, relayStats.acceptedDisconnects)});
  }
  if (relayStats.finalClientCount != 0) {
    diagnostics.push_back(
        {"error", "relay_not_drained",
         fmt::format("Expected relay final client count 0, observed {}",
                     relayStats.finalClientCount)});
  }
  if (totalConnectEvents(clients) < options.clientCount) {
    diagnostics.push_back(
        {"error", "missing_connect_events",
         fmt::format(
             "Expected at least {} observed connect events, observed {}",
             options.clientCount, totalConnectEvents(clients))});
  }
  if (totalDisconnectEvents(clients) < options.clientCount - 1) {
    diagnostics.push_back(
        {"error", "missing_disconnect_events",
         fmt::format(
             "Expected at least {} observed disconnect events, observed {}",
             options.clientCount - 1, totalDisconnectEvents(clients))});
  }

  for (const ClientSlot &client : clients) {
    if (client.remoteSnapshots < options.minRemoteSnapshots) {
      diagnostics.push_back(
          {"error", "missing_remote_snapshots",
           fmt::format(
               "Client {} expected at least {} remote snapshots, observed {}",
               client.clientId, options.minRemoteSnapshots,
               client.remoteSnapshots)});
    }
  }
}

nlohmann::json makeResultJson(const RunnerOptions &options,
                              const RelayStats &relayStats,
                              const std::vector<ClientSlot> &clients,
                              const std::vector<Diagnostic> &diagnostics,
                              std::chrono::milliseconds elapsed) {
  nlohmann::json clientJson = nlohmann::json::array();
  for (const ClientSlot &client : clients) {
    clientJson.push_back(clientSlotToJson(client));
  }

  nlohmann::json diagnosticJson = nlohmann::json::array();
  for (const Diagnostic &diagnostic : diagnostics) {
    diagnosticJson.push_back(diagnosticToJson(diagnostic));
  }

  return {
      {"host", "network_e2e_runner"},
      {"status", diagnostics.empty() ? "ok" : "error"},
      {"message",
       diagnostics.empty() ? "Network E2E completed" : "Network E2E failed"},
      {"clientCount", options.clientCount},
      {"snapshotsPerClient", options.snapshotsPerClient},
      {"minRemoteSnapshots", options.minRemoteSnapshots},
      {"bindHost", options.bindHost},
      {"port", options.port},
      {"elapsedMilliseconds", elapsed.count()},
      {"relay", relayStatsToJson(relayStats)},
      {"clients", std::move(clientJson)},
      {"diagnostics", std::move(diagnosticJson)},
  };
}

nlohmann::json makeErrorResultJson(std::string_view message) {
  return {
      {"host", "network_e2e_runner"},
      {"status", "error"},
      {"message", message},
      {"diagnostics",
       nlohmann::json::array({
           diagnosticToJson({"error", "process_error", std::string{message}}),
       })},
  };
}

int runNetworkE2E(const RunnerOptions &options,
                  const std::optional<std::filesystem::path> &resultFile) {
  const Clock::time_point started = Clock::now();
  std::vector<ClientSlot> clients;
  clients.reserve(options.clientCount);

  RelayRuntime relay{options.bindHost, options.port};
  relay.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  const vac::net::Endpoint serverEndpoint =
      vac::net::resolveEndpoint(options.bindHost, options.port);
  for (uint32_t index = 0; index < options.clientCount; ++index) {
    const uint8_t clientId = static_cast<uint8_t>(index + 1);
    clients.push_back({clientId, std::make_unique<vac::net::SnapshotClient>(
                                     clientId, serverEndpoint)});
    drainClientsFor(clients, std::chrono::milliseconds(20));
  }

  for (uint32_t tick = 0; tick < options.snapshotsPerClient; ++tick) {
    for (ClientSlot &client : clients) {
      if (client.client != nullptr) {
        client.client->sendSnapshot(
            snapshotFor(client.clientId, static_cast<uint16_t>(tick)));
      }
    }
    drainClientsOnce(clients);
    std::this_thread::sleep_for(std::chrono::milliseconds(options.drainStepMs));
  }

  const Clock::time_point remoteDeadline =
      Clock::now() + std::chrono::milliseconds(options.remoteTimeoutMs);
  while (!allClientsHaveRemoteSnapshots(clients, options.minRemoteSnapshots) &&
         Clock::now() < remoteDeadline) {
    drainClientsOnce(clients);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  for (size_t index = 0; index < clients.size(); ++index) {
    if (clients[index].client != nullptr) {
      clients[index].client->disconnect();
      clients[index].client.reset();
    }
    drainClientsFor(clients, std::chrono::milliseconds(25));
  }

  const Clock::time_point drainDeadline =
      Clock::now() + std::chrono::milliseconds(options.remoteTimeoutMs);
  RelayStats relayStats = relay.stats();
  while (relayStats.finalClientCount != 0 && Clock::now() < drainDeadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    relayStats = relay.stats();
  }

  relay.stop();
  relayStats = relay.stats();

  std::vector<Diagnostic> diagnostics = relayStats.diagnostics;
  appendFailureDiagnostics(options, relayStats, clients, diagnostics);

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now() - started);
  const nlohmann::json resultJson =
      makeResultJson(options, relayStats, clients, diagnostics, elapsed);
  if (resultFile.has_value()) {
    writeJsonFile(*resultFile, resultJson);
  }

  std::cout << fmt::format(
      "network_e2e_runner clients={} snapshots={} status={}\n",
      options.clientCount, options.snapshotsPerClient,
      diagnostics.empty() ? "ok" : "error");
  if (!diagnostics.empty()) {
    for (const Diagnostic &diagnostic : diagnostics) {
      std::cerr << fmt::format("{}: {}\n", diagnostic.code, diagnostic.message);
    }
  }

  return diagnostics.empty() ? 0 : 2;
}
} // namespace

int main(int argc, char **argv) {
  std::optional<std::filesystem::path> resultFile;
  try {
    vac::host::CommandLine commandLine{argc, argv};
    const vac::host::CommonHostOptions commonOptions =
        vac::host::parseCommonOptions(commandLine);
    resultFile = commonOptions.resultFile;
    vac::host::rejectUnsupportedCommonOptions(commonOptions,
                                              {"--result-file", "--headless"});
    const RunnerOptions options = parseRunnerOptions(commandLine);
    commandLine.rejectUnknown();

    return runNetworkE2E(options, resultFile);
  } catch (const std::exception &error) {
    std::cerr << "network_e2e_runner failed: " << error.what() << '\n';
    if (resultFile.has_value()) {
      try {
        writeJsonFile(*resultFile, makeErrorResultJson(error.what()));
      } catch (const std::exception &resultError) {
        std::cerr << "network_e2e_runner could not write result file: "
                  << resultError.what() << '\n';
      }
    }
    return 1;
  }
}
