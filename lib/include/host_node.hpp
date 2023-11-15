#pragma once

#include "network_node.hpp"
#include "network_interface.hpp"
#include "tcp/tcp_stack.hpp"


namespace tns {

// This network node represents a host.
class HostNode : public NetworkNode {
public:
    HostNode() = default;
    HostNode(const std::string_view lnxFile);
    HostNode(const std::string &lnxFile) : HostNode(std::string_view(lnxFile)) {};
    HostNode(const HostNode&) = delete;
    HostNode(HostNode&&) = delete;

    ~HostNode() {::THROW_NO_IMPL();}

    // Public API with TCP stack

    static constexpr auto tcpMaxPayloadSize() noexcept;

    // Create a socket and connect to a remote host (Active Open)
    auto tcpConnect(const ip::Ipv4Address &remoteIP, in_port_t remotePort);
 
    // Create a listening socket bound to the given port (Passive Open)
    auto tcpListen(in_port_t port);

    // Send a stream of data via a TCP socket.
    template <typename T>
    auto tcpSend(int socketID, const std::span<const T> data);

    // Receive data from a TCP socket into a buffer.
    auto tcpRecv(int socketID, const std::span<std::byte> buff);

    // List info of all sockets
    void tcpListSockets(std::ostream &os = std::cout);


private:
    void datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const override;

private:
    tcp::TcpStack tcpStack_;
};

} // namespace tns
