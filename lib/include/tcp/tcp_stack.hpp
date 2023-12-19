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
    using IpCallback = std::function<void(const ip::Ipv4Address &destIP, PayloadPtr payload)>;
    void registerIpCallback(IpCallback ipCallback) noexcept { sendIp_ = std::move(ipCallback); }

    // Create a socket and connect it to the given remote address (Active Open)
    // BLOCKS until the connection is established or an error occurs
    // Params: local IP address, remote IP address and port
    tl::expected<NormalSocketRef, SocketError>
    vConnect(const ip::Ipv4Address &local, const ip::Ipv4Address &remote)
    {
        // Get a free port number for local address
        const auto port = generatePortNumber_();

        // Create a new normal socket in ESTABLISHED state, or error
        return createActiveConnection_({
            .local  = {local.getAddrNetwork(), tns::util::hton(port)}, 
            .remote = remote
        });
    }

    // Create a listening socket bound to the given port (Passive Open)
    tl::expected<ListenSocketRef, SocketError> vListen(in_port_t port)
    {
        return createListenSocket_(port);
    }

    // Write socket info to an ostream. (Default is standard output.)
    void listSockets(std::ostream &os = std::cout) const
    {
        using namespace std;
        os  << setw(3)  << left  << "SID"    << " "   // SID
            << setw(15) << right << "LAddr"  << " "   // LAddr
            << setw(5)  << left  << "LPort"  << " "   // LPort
            << setw(15) << right << "RAddr"  << " "   // RAddr
            << setw(5)  << left  << "RPort"  << " "   // RPort
            << setw(12) << right << "Status" << "\n"; // Status

        std::shared_lock lock(socketTableMutex_);
        for (const auto &[_, socket] : socketTable_) {
            std::visit([&os](const auto &sock) {
                WriteInfo{}(sock, os);
            }, socket);
        }
    }

    // Find a socket by its id
    tl::expected<SocketRef, SocketError> findSocketNoLock(int id) noexcept
    {
        auto it = socketTable_.find(id);
        if (it == socketTable_.end())
            return tl::unexpected(SocketError::CONN_NOT_EXIST);
        return it->second;
    }
    auto findSocket(int id)
    {
        std::shared_lock lock(socketTableMutex_);
        return findSocketNoLock(id);
    }

    // Find a (normal) socket by its session tuple
    tl::expected<NormalSocketRef, SocketError> findNormalSocketNoLock(const SessionTuple &tuple) noexcept
    {
        auto it = sessionToSocket_.find(tuple);
        if (it == sessionToSocket_.end())
            return tl::unexpected(SocketError::CONN_NOT_EXIST);
        return it->second;
    }
    auto findNormalSocket(const SessionTuple &tuple)
    {
        std::shared_lock lock(socketTableMutex_);
        return findNormalSocketNoLock(tuple);
    }

    // Find a listen socket by port number
    tl::expected<ListenSocketRef, SocketError> findListenSocketNoLock(in_port_t port) noexcept
    {
        auto it = portToListenSocket_.find(port);
        if (it == portToListenSocket_.end())
            return tl::unexpected(SocketError::CONN_NOT_EXIST);
        return it->second;
    }
    auto findListenSocket(in_port_t port)
    {
        std::shared_lock lock(socketTableMutex_);
        return findListenSocketNoLock(port);
    }

    using ExpectedSize = NormalSocket::ExpectedSize;

    // Send data via a socket by its id
    template <typename T>
    ExpectedSize vSend(int id, const std::span<const T> data)
    {
        const auto sockMaybe = findSocket(id);
        if (!sockMaybe)
            return tl::unexpected{sockMaybe.error()};

        return std::visit(overload{
            [&](NormalSocket &sock)     { return sock.vSend(data); },
            [ ](auto &) -> ExpectedSize { return tl::unexpected{SocketError::NYI}; }
        }, sockMaybe->get());
    }

    ExpectedSize vRecv(int id, const std::span<std::byte> buff)
    {
        const auto sockMaybe = findSocket(id);
        if (!sockMaybe)
            return tl::unexpected{sockMaybe.error()};

        return std::visit(overload{
            [&](NormalSocket &sock)     { return sock.vRecv(buff, buff.size()); },
            [ ](auto &) -> ExpectedSize { return tl::unexpected{SocketError::NYI}; }
        }, sockMaybe->get());
    }

    // Close a socket by its id
    tl::expected<void, SocketError> vClose(int id)
    {
        auto sockMaybe = findSocket(id);
        if (!sockMaybe) return tl::unexpected{sockMaybe.error()};

        return std::visit([](auto &sock) { return sock.vClose(); }, sockMaybe->get());
    }

    // Abort a socket by its id
    tl::expected<void, SocketError> vAbort(int id)
    {
        auto sockMaybe = findSocket(id);
        if (!sockMaybe) return tl::unexpected{sockMaybe.error()};

        return std::visit([](auto &sock) { return sock.vAbort(); }, sockMaybe->get());
    }

    void tcpProtocolHandler(DatagramPtr datagram)
    {
        // Construct TCP packet from datagram payload
        const auto packet = Packet::makePacketFromPayload(
            datagram->getSrcAddr().getAddrNetwork(),
            datagram->getDstAddr().getAddrNetwork(),
            datagram->getPayloadView()
        );
        if (!packet) {
            // Invalid checksum or other error
            std::stringstream ss;
            ss << "TcpStack::tcpProtocolHandler(): Discarding TCP packet: " << packet.error() << "\n";
            std::cerr << ss.str();
            return;
        }

        // Swap the session tuple
        const SessionTuple sess {
            .local  = { datagram->getDstAddr().getAddrNetwork(), packet->getDstPortNetwork() },
            .remote = { datagram->getSrcAddr().getAddrNetwork(), packet->getSrcPortNetwork() }
        };

        // Convert Packet to Event
        const auto eventMaybe = events::fromPacket(*packet, sess);
        if (!eventMaybe) {
            std::stringstream ss;
            ss << "TcpStack::tcpProtocolHandler(): Discarding TCP packet: " << eventMaybe.error() << "\n";
            std::cerr << ss.str();
            return;
        }

        if (auto ns = findNormalSocket(sess); ns) {
            // The connection/session already exists, invoke the appropriate handler on the normal socket
            auto &sock = ns->get();
            std::visit([this, &sock](const auto &state, const auto &event) {
                eventHandler_(sock, state, event);
            }, sock.state_, *eventMaybe);
        }
        else if (auto ls = findListenSocket(sess.local.getPortHost()); ls) {
            // No existing connection found, but a listen socket is listening on the port
            // Invoke the appropriate handler on the listen socket
            auto &lSock = ls->get();
            std::visit([this, &lSock](const auto &state, const auto &event) {
                eventHandler_(lSock, state, event);
            }, lSock.state_, *eventMaybe);
        }
        else {
            // No matching socket found
            std::stringstream ss;
            ss << "TcpStack::tcpProtocolHandler(): No matching socket found for TCP packet from " 
               << sess.remote.toString() << " (remote) to " << sess.local.toString() << " (local)\n";
            std::cerr << ss.str();
        }
    }

    void sendPacket(const Packet &packet, const ip::Ipv4Address &destAddr) const
    {
        sendIp_(destAddr, packet.serialize());
    }

private:
    IpCallback sendIp_ = [](const ip::Ipv4Address &, PayloadPtr) {};     // Default nop

    std::map<int, Socket> socketTable_;
    std::unordered_map<SessionTuple, NormalSocketRef> sessionToSocket_;  // Normal sockets (Pending or Established)
    std::unordered_map<in_port_t, ListenSocketRef> portToListenSocket_;  // Listening sockets
    mutable std::shared_mutex socketTableMutex_;

    // TODO!: Reaper thread to purge closed/timed-wait sockets
    tns::util::threading::PeriodicThread reaperThread_{SOCKET_REAPER_THREAD_PERIOD, [this]() {
        using namespace std;
        static constexpr auto reapable = [](const auto &state) {
            if (holds_alternative<states::Closed>(state)) return true;
            if (const auto *ps = get_if<states::TimeWait>(&state); ps && ps->isExpired()) return true;
            return false;
        };

        lock_guard lock(socketTableMutex_);
        for (auto it = socketTable_.begin(); it != socketTable_.end(); ) {
            visit(overload{
                [&](NormalSocket &sock) {
                    if (reapable(sock.state_)) {
                        sessionToSocket_.erase(sock.tuple_);
                        freeSocketId_(sock.id_);
                        it = socketTable_.erase(it);
                    } else {
                        it++;
                    }
                },
                [&](ListenSocket &lSock) {
                    if (holds_alternative<states::Closed>(lSock.state_)) {
                        portToListenSocket_.erase(lSock.port_);
                        freeSocketId_(lSock.id_);
                        it = socketTable_.erase(it);
                    } else {
                        it++;
                    }
                },
            }, it->second);
        }
    }};

    static constexpr int MAX_SOCKET_FD = 128;
    std::set<int> freeSocketIDs_ = []() {  // {1 .. MAX_SOCKET_FD}
        std::set<int> s;
        std::ranges::for_each(std::views::iota(1, MAX_SOCKET_FD + 1),
                              [&, it=s.begin()](int id) mutable { it = s.insert(it, id); });
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
                             uint32_t clientWND, ListenSocket &listener)  // clientISN, clientWND: host byte order
    {
        std::cout << "TcpStack::createPassiveConnection_(): "
                  << "(Local = "   << tuple.local.toString()
                  << ", Remote = " << tuple.remote.toString()
                  << ", clientISN = " << clientISN
                  << ", listen socket = " << listener.id_ << ")\n";

        // Create a new normal socket
        auto sockMaybe = createNormalSocket_(tuple, clientISN + 1, clientWND);  // Closed initially, random ISN, rcvNxt = clientISN + 1
        if (!sockMaybe)
            return tl::unexpected(sockMaybe.error());
        auto &sock = sockMaybe->get();

        const auto seq = sock.sendBuffer_.getNxt();
        const auto [ack, wnd] = sock.recvBuffer_.getAckWnd();

        std::cout << "TcpStack::createPassiveConnection_(): Created normal socket " << sock.id_ << " (CLOSED)\n";
        std::cout << "TcpStack::createPassiveConnection_(): Sending SYN-ACK ("
                  <<   "seq = " << seq << ", ack = " << ack << ", wnd = " << wnd << ") ...\n";

        // Send SYN-ACK reply packet back to tuple.remote, ACK = clientISN + 1
        sock.sendBuffer_.writeAndSendOne();  // ugly hack to increment SND.NBW and SND.NXT by one
        sock.sendPacket_(Packet::makeSynAckPacket(tuple, seq, ack, static_cast<uint16_t>(wnd)));

        // Add socket to the pending connections list of the listener
        if ( !listener.pendingSocks_.add(tuple, sock) )
            return tl::unexpected(SocketError::NO_RESOURCES);

        // Then transition the socket state to SYN_RECEIVED (TODO: race condition?)
        sock.state_ = states::SynReceived{listener};  // Record the listener that asked to create this socket

        std::cout << "TcpStack::createPassiveConnection_(): SYN-ACK sent. Socket " << sock.id_ << " -> SYN_RECEIVED\n";

        return sock;
    }

    // Create an active connection (client side) by sending a SYN request to the remote
    // BLOCKS until the connection is established or an error occurs. Returns the resulting socket or error
    tl::expected<NormalSocketRef, SocketError>
    createActiveConnection_(const SessionTuple &tuple)
    {
        std::cout << "TcpStack::createActiveConnection_(): "
                  << "(Local = "   << tuple.local.toString()
                  << ", Remote = " << tuple.remote.toString() << ")\n";

        // Create a new normal socket
        auto sockMaybe = createNormalSocket_(tuple);  // Closed initially, random ISN
        if (!sockMaybe)
            return tl::unexpected(sockMaybe.error());
        auto &sock = sockMaybe->get();

        const auto seq = sock.sendBuffer_.getNxt();
        const auto wnd = static_cast<uint16_t>(sock.recvBuffer_.getSizeFree());
        std::cout << "TcpStack::createActiveConnection_(): Sending SYN (seq = " << seq << ", wnd = " << wnd << ") ...\n";

        // Send SYN packet to the remote
        sock.sendBuffer_.writeAndSendOne();  // ugly hack to increment SND.NBW and SND.NXT by one
        sock.sendPacket_(Packet::makeSynPacket(tuple, seq, wnd));

        std::cout << "TcpStack::createActiveConnection_(): Waiting for the SYN-ACK Reply...\n";

        // Transition the socket state to SYN_SENT (TODO: race condition?)
        states::SynSent::SynAckResult result{};
        sock.state_ = states::SynSent{result};

        // Wait for SYN-ACK reply
        if (const auto err = result.waitForSynAck(); err) {
            std::cout << "\tTcpStack::createActiveConnection_(): Socket " << sock.id_ << " failed to connect: " << *err << "\n";
            return tl::unexpected{*err};
        }

        // Got SYN-ACK
        std::cout << "\tTcpStack::createActiveConnection_(): Got SYN-ACK, and we've replied ACK! Connection established on socket " << sock.id_ << "!!!\n";

        assert(std::holds_alternative<states::SynSent>(sock.state_) && "FATAL: Socket state should still be in SynSent");
        assert(sock.sendBuffer_.sanityCheckAtStart() && "FATAL: sendBuffer_ is not sane");
        assert(sock.recvBuffer_.sanityCheckAtStart() && "FATAL: recvBuffer_ is not sane");

        // Transition the socket state to ESTABLISHED (TODO: race condition?)
        sock.state_ = states::Established{};

        return sock;
    }

    tl::expected<int, SocketError> getNextSocketIdNoLock_() noexcept 
    {
        if (freeSocketIDs_.empty())
            return tl::unexpected(SocketError::NO_RESOURCES);
        int id = *freeSocketIDs_.begin();  // Get the first free socket id
        freeSocketIDs_.erase(freeSocketIDs_.begin());
        return id;
    }
    auto getNextSocketId_()
    {
        std::lock_guard lock(freeSocketsMutex_);
        return getNextSocketIdNoLock_();
    }

    // Create a new listen socket in the socket table
    tl::expected<ListenSocketRef, SocketError> createListenSocket_(in_port_t port)  // port: host byte order
    {
        {
            std::shared_lock lock(socketTableMutex_);
            if (portToListenSocket_.contains(port))
                return tl::unexpected(SocketError::DUPLICATE_SOCKET);
        }

        auto id = getNextSocketId_();
        if (!id) return tl::unexpected(id.error());

        {
            std::unique_lock lock(socketTableMutex_);
            auto [sockTableIt, ok1] = socketTable_.try_emplace(  // Create a new listen socket
                *id, 
                std::in_place_type<ListenSocket>, 
                *id, port, ListenSocket::CtorToken{}
            );
            assert(ok1);

            // Update mapping (port, listenSocketRef)
            auto [mappingIt, ok2] = portToListenSocket_.emplace(
                port, std::get<ListenSocket>(sockTableIt->second));
            assert(ok2);

            return mappingIt->second;
        }
    }

    // Create a new normal socket in the socket table
    tl::expected<NormalSocketRef, SocketError> 
    createNormalSocket_(const SessionTuple &tuple, uint32_t rcvNxt = 0, 
                        uint32_t windowSize = std::numeric_limits<uint32_t>::max())
    {
        // Errors if there is already a socket with the same tuple
        {
            std::shared_lock lock(socketTableMutex_);
            if (sessionToSocket_.contains(tuple))
                return tl::unexpected(SocketError::DUPLICATE_SOCKET);
        }

        // Get socket id
        auto id = getNextSocketId_();
        if (!id) return tl::unexpected(id.error());

        // Callbacks for the normal socket
        NormalSocket::TcpStackCallbacks callbacks{
            .sendPacket = [this](const Packet &packet, const ip::Ipv4Address &destAddr) { sendPacket(packet, destAddr); }
        };

        // Create a new normal socket
        std::unique_lock lock(socketTableMutex_);

        auto [sockTableIt, sockTableSuccess] = socketTable_.try_emplace(
            *id,
            std::in_place_type<NormalSocket>,
            *id, tuple, 
            generateISN_(), windowSize,  // sendBuffer_
            rcvNxt,                      // recvBuffer_
            std::move(callbacks),
            NormalSocket::CtorToken{}
        );
        assert(sockTableSuccess);

        // Update mapping (sessionTuple, normalSocketRef)
        auto [it, success] = sessionToSocket_.emplace(tuple, std::get<NormalSocket>(sockTableIt->second));
        assert(success);

        return it->second;
    }

    // Takes in an iterator to a socket in the socket table and purges it.
    // Returns the iterator to the next socket in the socket table.
    // Assume the socket is in the socket table, i.e., it != socketTable_.end()
    // auto purgeSocket_( decltype(socketTable_.begin()) it )
    // {
    //     assert(it != socketTable_.end() && "FATAL: Socket not found in socket table");

    //     auto &sock = it->second;
    //     std::visit([&](auto &sock) {
    //         using T = std::decay_t<decltype(sock)>;
    //         if constexpr (std::is_same_v<T, NormalSocket>) {
    //             sessionToSocket_.erase(sock.tuple_);
    //         }
    //         else if constexpr (std::is_same_v<T, ListenSocket>) {
    //             portToListenSocket_.erase(lSock.port_);
    //         }
    //         else {
    //             static_assert(util::always_false_v<T>, "non-exhaustive visitor!");
    //         }
    //     }, sock);
    // }

    // // Assume sock IS in the socket table
    // void purgeSocket_(NormalSocket &sock)
    // {
    //     assert(std::holds_alternative<states::Closed>(sock.state_) 
    //            && "FATAL: Normal socket state should be Closed before purge");

    //     const auto id = sock.id_;
    //     {
    //         // Remove the socket from the mapping and the socket table 
    //         std::unique_lock lock(socketTableMutex_);
    //         sessionToSocket_.erase(sock.tuple_);
    //         socketTable_.erase(id);
    //     }
    //     freeSocketId_(id);
    // }

    // void purgeSocket_(ListenSocket &lSock)
    // {
    //     assert(std::holds_alternative<states::Closed>(lSock.state_) 
    //            && "FATAL: Listen socket state should be Closed before purge");

    //     const auto id = lSock.id_;
    //     {
    //         // Remove the socket from the mapping and the socket table 
    //         std::unique_lock lock(socketTableMutex_);
    //         portToListenSocket_.erase(lSock.port_);
    //         socketTable_.erase(id);
    //     }
    //     freeSocketId_(id);
    // }

    void freeSocketId_(int id)
    {
        std::lock_guard lock(freeSocketsMutex_);
        freeSocketIDs_.insert(id);
    }

    uint32_t generateISN_() const noexcept
    {
        return isnDist_(rng_);
    }

    in_port_t generatePortNumber_() const noexcept
    {
        // TODO: Make sure it's a FREE port
        return portNumDist_(rng_);
    }


    /******************************************** Normal Socket ********************************************/
    // Active OPEN
    void eventHandler_(NormalSocket&, const states::SynSent     &, const events::GetSynAck &); // SYN_SENT => ESTABLISHED

    // Passive OPEN
    void eventHandler_(NormalSocket&, const states::SynReceived &, const events::GetAck    &); // SYN_RECEIVED => ESTABLISHED

    // ESTABLISHED => ESTABLISHED
    void eventHandler_(NormalSocket&, const states::Established &, const events::GetAck    &); // Process data
    void eventHandler_(NormalSocket&, const states::Established &, const events::GetSynAck &); // Retransmitted SYN-ACK

    // Passive CLOSE
    void eventHandler_(NormalSocket&, const states::Established &, const events::GetFin    &); // ESTABLISHED => CLOSE_WAIT
    void eventHandler_(NormalSocket&, const states::Established &, const events::GetFinAck &); // ESTABLISHED => CLOSE_WAIT
    void eventHandler_(NormalSocket&, const states::CloseWait   &, const events::GetAck    &); // Process data
    void eventHandler_(NormalSocket&, const states::CloseWait   &, const events::GetFin    &); // Retransmitted FIN
    void eventHandler_(NormalSocket&, const states::LastAck     &, const events::GetAck    &); // LAST_ACK => CLOSED

    // Active CLOSE
    void eventHandler_(NormalSocket&, const states::FinWait1    &, const events::GetAck    &); // FIN_WAIT_1 => FIN_WAIT_2
    void eventHandler_(NormalSocket&, const states::FinWait2    &, const events::GetFin    &); // FIN_WAIT_2 => TIME_WAIT
    void eventHandler_(NormalSocket&, const states::FinWait2    &, const events::GetAck    &); // Process data
    void eventHandler_(NormalSocket&, const states::TimeWait    &, const events::GetFin    &); // Retransmitted FIN

    // Unsupported transitions
    void eventHandler_(NormalSocket&, const auto&, const auto&) { return; }

    /********************************************* Listen Socket *********************************************/
    void eventHandler_(ListenSocket&, const states::Listen&, const events::GetSyn&);
    void eventHandler_(ListenSocket&, const auto&, const auto&) { return; }       // Unsupported transitions
};

} // namespace tcp
} // namespace tns