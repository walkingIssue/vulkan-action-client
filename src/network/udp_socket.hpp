#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#error "UDP transport currently has a Windows Winsock implementation only."
#endif

namespace vac::net
{
struct Endpoint
{
    sockaddr_storage address{};
    int length = 0;
};

struct Datagram
{
    Endpoint sender;
    std::array<std::byte, 1500> bytes{};
    size_t size = 0;
};

Endpoint resolveEndpoint(const std::string &hostAndPort);
Endpoint resolveEndpoint(const std::string &host, uint16_t port, bool passive = false);
std::string endpointToString(const Endpoint &endpoint);
bool sameEndpoint(const Endpoint &a, const Endpoint &b);

class UdpSocket
{
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;

    UdpSocket(UdpSocket &&other) noexcept;
    UdpSocket &operator=(UdpSocket &&other) noexcept;

    void open(const std::string &bindHost, uint16_t bindPort, bool nonBlocking);
    void setNonBlocking(bool enabled);
    void close();

    size_t sendTo(std::span<const std::byte> bytes, const Endpoint &endpoint);
    std::optional<Datagram> receive();

private:
#ifdef _WIN32
    SOCKET m_socket = INVALID_SOCKET;
#endif
};
} // namespace vac::net
