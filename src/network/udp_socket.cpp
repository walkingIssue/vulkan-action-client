#include "network/udp_socket.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace vac::net
{
namespace
{
class WinsockSession
{
public:
    WinsockSession()
    {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~WinsockSession()
    {
        WSACleanup();
    }
};

void ensureWinsock()
{
    static WinsockSession session;
}

std::string winsockError(std::string_view operation)
{
    return fmt::format("{} failed with WSA error {}", operation, WSAGetLastError());
}

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
} // namespace

Endpoint resolveEndpoint(const std::string &hostAndPort)
{
    const auto [host, port] = splitHostPort(hostAndPort);
    return resolveEndpoint(host, port);
}

Endpoint resolveEndpoint(const std::string &host, uint16_t port, bool passive)
{
    ensureWinsock();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = passive ? AI_PASSIVE : 0;

    addrinfo *result = nullptr;
    const std::string portString = std::to_string(port);
    const int rc = getaddrinfo(host.empty() ? nullptr : host.c_str(), portString.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        throw std::runtime_error(fmt::format("Could not resolve '{}:{}': {}", host, port, rc));
    }

    Endpoint endpoint;
    std::memcpy(&endpoint.address, result->ai_addr, static_cast<size_t>(result->ai_addrlen));
    endpoint.length = static_cast<int>(result->ai_addrlen);
    freeaddrinfo(result);
    return endpoint;
}

std::string endpointToString(const Endpoint &endpoint)
{
    char host[NI_MAXHOST]{};
    char service[NI_MAXSERV]{};
    const int rc = getnameinfo(reinterpret_cast<const sockaddr *>(&endpoint.address),
                               endpoint.length,
                               host,
                               sizeof(host),
                               service,
                               sizeof(service),
                               NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
        return "<invalid-endpoint>";
    }
    return fmt::format("{}:{}", host, service);
}

bool sameEndpoint(const Endpoint &a, const Endpoint &b)
{
    return a.length == b.length &&
           std::memcmp(&a.address, &b.address, static_cast<size_t>(a.length)) == 0;
}

UdpSocket::~UdpSocket()
{
    close();
}

UdpSocket::UdpSocket(UdpSocket &&other) noexcept
    : m_socket(std::exchange(other.m_socket, INVALID_SOCKET))
{
}

UdpSocket &UdpSocket::operator=(UdpSocket &&other) noexcept
{
    if (this != &other) {
        close();
        m_socket = std::exchange(other.m_socket, INVALID_SOCKET);
    }
    return *this;
}

void UdpSocket::open(const std::string &bindHost, uint16_t bindPort, bool nonBlocking)
{
    ensureWinsock();
    close();

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        throw std::runtime_error(winsockError("socket"));
    }

    const Endpoint bindEndpoint = resolveEndpoint(bindHost, bindPort, true);
    if (bind(m_socket,
             reinterpret_cast<const sockaddr *>(&bindEndpoint.address),
             bindEndpoint.length) == SOCKET_ERROR) {
        const std::string error = winsockError("bind");
        close();
        throw std::runtime_error(error);
    }

    setNonBlocking(nonBlocking);
}

void UdpSocket::setNonBlocking(bool enabled)
{
    if (m_socket == INVALID_SOCKET) {
        return;
    }

    u_long mode = enabled ? 1u : 0u;
    if (ioctlsocket(m_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        throw std::runtime_error(winsockError("ioctlsocket"));
    }
}

void UdpSocket::close()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

size_t UdpSocket::sendTo(std::span<const std::byte> bytes, const Endpoint &endpoint)
{
    const int sent = sendto(m_socket,
                            reinterpret_cast<const char *>(bytes.data()),
                            static_cast<int>(bytes.size()),
                            0,
                            reinterpret_cast<const sockaddr *>(&endpoint.address),
                            endpoint.length);
    if (sent == SOCKET_ERROR) {
        throw std::runtime_error(winsockError("sendto"));
    }
    return static_cast<size_t>(sent);
}

std::optional<Datagram> UdpSocket::receive()
{
    Datagram datagram;
    datagram.sender.length = sizeof(datagram.sender.address);
    const int received = recvfrom(m_socket,
                                  reinterpret_cast<char *>(datagram.bytes.data()),
                                  static_cast<int>(datagram.bytes.size()),
                                  0,
                                  reinterpret_cast<sockaddr *>(&datagram.sender.address),
                                  &datagram.sender.length);
    if (received == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) {
            return std::nullopt;
        }
        throw std::runtime_error(winsockError("recvfrom"));
    }

    datagram.size = static_cast<size_t>(received);
    return datagram;
}
} // namespace vac::net
