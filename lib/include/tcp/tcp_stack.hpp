#pragma once

#include "util/tl/expected.hpp"
#include "ip/datagram.hpp"
#include "ip/address.hpp"
#include "tcp/sockets.hpp"
#include "tcp/packet.hpp"
#include "tcp/states.hpp"

#include <set>
#include <shared_mutex>
#include <algorithm>
#include <random>
#include <map>
#include <iostream>
#include <iomanip>
#include <limits>

namespace tns {
namespace tcp {

class TcpStack {
public:
    TcpStack() = default;
    TcpStack(const TcpStack&) = delete;
    TcpStack(TcpStack&&) = delete;

    ~TcpStack() {::THROW_NO_IMPL();}

    using IpCallback = std::function<void(const ip::Ipv4Address &destIP, PayloadPtr payload)>;
    void registerIpCallback(IpCallback ipCallback) noexcept;

    // Create a socket and connect it to the given remote address (Active Open)
    // BLOCKS until the connection is established or an error occurs
    // Params: local IP address, remote IP address and port
    tl::expected<NormalSocketRef, SocketError>
    vConnect(const ip::Ipv4Address &local, const ip::Ipv4Address &remote);

    // Create a listening socket bound to the given port (Passive Open)
    tl::expected<ListenSocketRef, SocketError> vListen(in_port_t port);

    // Write socket info to an ostream. (Default is standard output.)
    void listSockets(std::ostream &os = std::cout) const;

    // Find a socket by its id
    tl::expected<SocketRef, SocketError> findSocketNoLock(int id) noexcept;
    auto findSocket(int id);

    // Find a (normal) socket by its session tuple
    tl::expected<NormalSocketRef, SocketError> findNormalSocketNoLock(const SessionTuple &tuple) noexcept;
    auto findNormalSocket(const SessionTuple &tuple);

    // Find a listen socket by port number
    tl::expected<ListenSocketRef, SocketError> findListenSocketNoLock(in_port_t port) noexcept;
    auto findListenSocket(in_port_t port);

    // Send data via a socket by its id
    template <typename T>
    tl::expected<std::size_t, SocketError> vSend(int id, const std::span<const T> data);

    tl::expected<std::size_t, SocketError> vRecv(int id, const std::span<std::byte> buff);

    // Close a socket by its id
    bool vClose(int id);

    void tcpProtocolHandler(DatagramPtr datagram);

    void sendPacket(const Packet &packet, const ip::Ipv4Address &destAddr) const;

private:
    IpCallback sendIp_ = [](const ip::Ipv4Address &, PayloadPtr) {};     // Default nop

    std::map<int, Socket> socketTable_;
    std::unordered_map<SessionTuple, NormalSocketRef> sessionToSocket_;  // Normal sockets (Pending or Established)
    std::unordered_map<in_port_t, ListenSocketRef> portToListenSocket_;  // Listening sockets
    mutable std::shared_mutex socketTableMutex_;

    static constexpr int MAX_SOCKET_FD = 128;
    std::set<int> freeSocketIDs_ = []() {  // {1 .. MAX_SOCKET_FD}
        std::set<int> s;
        auto it = s.begin();
        for (int i = 1; i <= MAX_SOCKET_FD; i++) 
            it = s.insert(it, i);
        return s;
    }();
    mutable std::mutex freeSocketsMutex_;

    // std::mt19937 rng_{std::random_device{}()};
    mutable std::mt19937 rng_{0};  // set seed for debugging
    mutable std::uniform_int_distribution<uint32_t> isnDist_{0, std::numeric_limits<uint32_t>::max()};
    mutable std::uniform_int_distribution<in_port_t> portNumDist_{1024, std::numeric_limits<in_port_t>::max()};

private:
    // Create a passive connection (server side) due to a SYN request from a client
    tl::expected<NormalSocketRef, SocketError>
    createPassiveConnection_(const SessionTuple &tuple, uint32_t clientISN, 
                             uint32_t clientWDN, ListenSocket &listener);  // clientISN, clientWDN: host byte order

    // Create an active connection (client side) by sending a SYN request to the remote
    // BLOCKS until the connection is established or an error occurs. Returns the resulting socket or error
    tl::expected<NormalSocketRef, SocketError>
    createActiveConnection_(const SessionTuple &tuple);

    tl::expected<int, SocketError> getNextSocketIdNoLock_() noexcept;
    auto getNextSocketId_();

    // Create a new listen socket in the socket table
    tl::expected<ListenSocketRef, SocketError> createListenSocket_(in_port_t port);  // port: host byte order

    // Create a new normal socket in the socket table
    tl::expected<NormalSocketRef, SocketError> 
    createNormalSocket_(const SessionTuple &tuple, uint32_t rcvNxt = 0, 
                        uint32_t windowSize = std::numeric_limits<uint32_t>::max());

    // Assume sock IS in the socket table
    void removeNormalSocket_(NormalSocket &sock);

    uint32_t generateISN_() const noexcept;
    in_port_t generatePortNumber_() const noexcept;


    /******************************************** Normal Socket ********************************************/
    void eventHandler_(NormalSocket &sock, const states::SynSent &synSent, const events::GetSynAck &getSynAck);  // --> ESTABLISHED
    void eventHandler_(NormalSocket &sock, const states::SynReceived &synRecv, const events::GetAck &getAck);  // --> ESTABLISHED
    void eventHandler_(NormalSocket &sock, const states::Established &estab, const events::GetAck &getAck);    // Process data
    void eventHandler_(NormalSocket &, const auto&, const auto&) { return; }

    /********************************************* Listen Socket *********************************************/
    void eventHandler_(ListenSocket &lSock, const events::GetSyn &getSyn);  // --> SYN_RECEIVED
    void eventHandler_(ListenSocket &, const auto &) { return; }       // Unsupported transitions
};

} // namespace tcp
} // namespace tns