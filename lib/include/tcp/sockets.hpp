#pragma once

#include "tcp/session_tuple.hpp"
#include "tcp/states.hpp"
#include "tcp/packet.hpp"
#include "tcp/socket_error.hpp"
#include "tcp/buffers.hpp"
#include "tcp/constants.hpp"
#include "ip/address.hpp"
#include "util/tl/expected.hpp"
#include "util/defines.hpp"

#include <iomanip>
#include <variant>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <netinet/tcp.h>
#include <optional>
#include <span>

namespace tns {
namespace tcp {

/* Forward declarations */
class NormalSocket;
class ListenSocket;

using Socket          = std::variant<NormalSocket, ListenSocket>;
using SocketRef       = std::reference_wrapper<Socket>;
using NormalSocketRef = std::reference_wrapper<NormalSocket>;
using ListenSocketRef = std::reference_wrapper<ListenSocket>;


/* Visitors for Socket */
struct WriteInfo {
    void operator()(const NormalSocket &sock,  std::ostream &os) const;
    void operator()(const ListenSocket &lSock, std::ostream &os) const;
};

class NormalSocket {

    struct CtorToken {};  // passkey idiom
    struct TcpStackCallbacks {
        std::function<void(const Packet &packet, const ip::Ipv4Address &destAddr)> sendPacket;
    };

public:
    NormalSocket(int id, const SessionTuple &tuple, 
                 uint32_t isn, uint32_t windowSize, // sendBuffer_
                 uint32_t rcvNxt,  // recvBuffer_
                 TcpStackCallbacks callbacks,
                 CtorToken);

    NormalSocket() = delete;
    NormalSocket(const NormalSocket&) = delete;
    NormalSocket(NormalSocket&&) = delete;

    ~NormalSocket() {::THROW_NO_IMPL();}

    template <typename T>
    tl::expected<std::size_t, SocketError> vSend(const std::span<const T> data);

    tl::expected<std::size_t, SocketError> vRecv(const std::span<std::byte> buff, std::size_t n);
    
    bool vClose();

    // Getters
    int getID() const;
    const SessionTuple & getSessionTuple() const;

private:
    int id_;  // socket ID used by the kernel to identify the socket: This is NOT the same as the file descriptor
    SessionTuple tuple_;

    State state_ = states::Closed{};

    // send and recv buffers
    SendBuffer<SEND_BUFFER_SIZE> sendBuffer_;
    RecvBuffer<RECV_BUFFER_SIZE> recvBuffer_;

    // Callbacks to the TCP stack (sendPacket, etc.)
    const TcpStackCallbacks tcpStackCallbacks_;

    friend WriteInfo;
    friend class TcpStack;

    void senderFunction_();
};


class ListenSocket {

    struct CtorToken {};  // passkey idiom

public:
    ListenSocket(int id, in_port_t port, CtorToken);

    ListenSocket() = delete;
    ListenSocket(const ListenSocket&) = delete;
    ListenSocket(ListenSocket&&) = delete;

    // The destructor of the socket will be called only when it gets removed from the TCP stack.
    // Either: TcpStack::vClose() or TcpStack::vCloseAll()
    ~ListenSocket() {::THROW_NO_IMPL();}

    // Accept connection by dequeueing an established socket from the acceptQ_
    // BLOCKS until a new connection is available or an error occurs
    tl::expected<NormalSocketRef, SocketError> vAccept() noexcept;

    bool vClose();

    // Getters
    int getID() const;
    in_port_t getPort() const;

private:
    int id_;
    in_port_t port_;  // host byte order
    // NOTE: We always assume that the listen socket is bound to all interfaces (addr_ == INADDR_ANY)

    bool closed = false;

    // A list of pending connections (SYN-RECEIVED sockets) keyed by the session tuple
    static constexpr std::size_t MAX_PENDING_SOCKS = 64;
    struct {
        mutable std::mutex mutex;
        using PendingSock = std::pair<tcp::SessionTuple, NormalSocketRef>;  // <session tuple, socket>
        std::vector<PendingSock> socks;  // Pending connections

        void onClose();

        // Adds a new connection to the pending connections list
        auto add(const tcp::SessionTuple &sess, NormalSocketRef sock);

        // Returns the socket and removes it from the pending connections list
        std::optional<NormalSocketRef> remove(const tcp::SessionTuple &sess);

    } pendingSocks_;

    // A queue of established sockets waiting to be accepted
    struct {
        std::queue<NormalSocketRef> queue;
        std::mutex queueMutex;
        std::condition_variable queueCondVar;
        bool closed = false;

        void onClose();
        void pushAndNotify(NormalSocketRef sock);
        NormalSocketRef waitAndPop();
    } acceptQ_;

    friend WriteInfo;
    friend class TcpStack;
};

} // namespace tcp 
} // namespace tns
