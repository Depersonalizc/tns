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

class ListenSocket {

    struct CtorToken {};  // passkey idiom

public:
    ListenSocket(int id, in_port_t port, CtorToken) : id_(id), port_(port) {}

    // ListenSocket() = delete;
    // ListenSocket(const ListenSocket&) = delete;
    // ListenSocket(ListenSocket&&) = delete;

    ~ListenSocket()
    {
        std::cout << "ListenSocket::~ListenSocket: Listen socket " << id_ << " is being destroyed\n";
        std::cout << "ListenSocket::~ListenSocket: DONE!\n";
    }

    // Accept connection by dequeueing an established socket from the acceptQ_
    // BLOCKS until a new connection is available or an error occurs
    tl::expected<NormalSocketRef, SocketError> vAccept() noexcept
    {
        return acceptQ_.waitAndPop();
    }

    tl::expected<void, SocketError> vClose()
    {
        using ReturnType = tl::expected<void, SocketError>;

        return std::visit(overload{
            [this](const states::Listen &) -> ReturnType {
                pendingSocks_.onClose();  // Close all pending connections; Remove from socket table?
                acceptQ_.onClose();       // Empty the queue & wake up all threads waiting on the queue
                state_ = states::Closed{};
                return {};
            },
            [](const states::Closed &) -> ReturnType { return tl::unexpected{SocketError::CONN_NOT_EXIST}; },
            [](const auto &) -> ReturnType { throw std::logic_error{"ListenSocket::vClose() called in an invalid state"}; }
        }, state_);
    }

    tl::expected<void, SocketError> vAbort() { return vClose(); }

    // Getters
    int getID() const { return id_; }
    in_port_t getPort() const { return port_; }

private:
    int id_;
    in_port_t port_;  // host byte order
    State state_ = states::Listen{};  // LISTEN/CLOSED

    // A list of pending connections (SYN-RECEIVED sockets) keyed by the session tuple
    struct PendingSocks {
        static constexpr std::size_t MAX_PENDING_SOCKS = 64;
        mutable std::mutex mutex;

        using PendingSock = std::pair<tcp::SessionTuple, NormalSocketRef>;  // <session tuple, socket>
        std::vector<PendingSock> socks;  // Pending connections

        ~PendingSocks() { onClose(); }

        void onClose();
    
        // Adds a new connection to the pending connections list
        auto add(const tcp::SessionTuple &sess, NormalSocketRef sock) -> std::optional<PendingSock>;

        // Returns the socket and removes it from the pending connections list
        auto remove(const tcp::SessionTuple &sess) -> std::optional<NormalSocketRef>;
    } pendingSocks_;

    // A queue of ESTABLISHED sockets waiting to be accepted
    struct AcceptQueue {
        std::queue<NormalSocketRef> queue;
        std::mutex queueMutex;
        std::condition_variable queueCondVar;
        bool closed = false;

        ~AcceptQueue() { onClose(); }

        void onClose();
        void pushAndNotify(NormalSocketRef sock);
        auto waitAndPop() -> tl::expected<NormalSocketRef, SocketError>;
    } acceptQ_;

    friend WriteInfo;
    friend class TcpStack;
    friend class NormalSocket;
};

class NormalSocket {

    using NS = NormalSocket;
    struct CtorToken {};  // passkey idiom
    struct TcpStackCallbacks {
        std::function<void(const Packet &packet, const ip::Ipv4Address &destAddr)> sendPacket;
    };

public:
    NormalSocket(int id, const SessionTuple &tuple, 
                 uint32_t isn, uint32_t windowSize, // sendBuffer_
                 uint32_t rcvNxt,  // recvBuffer_
                 TcpStackCallbacks callbacks,
                 CtorToken)
        : id_{id}, tuple_{tuple}
        , sendBuffer_(isn, windowSize)
        , recvBuffer_(rcvNxt)
        // , senderThread_{&NS::senderFunction_, this}
        // , retransmitThread_{RETRANSMIT_THREAD_PERIOD, &NS::retransmitFunction_, this}
        // , zwpThread_{&NS::zwpFunction_, this}
        , tcpStackCallbacks_{std::move(callbacks)}
    {}

    ~NormalSocket()
    {
        std::cout << "NormalSocket::~NormalSocket: Normal socket " << id_ << " is being destroyed\n";
        shutdown_();
        std::cout << "NormalSocket::~NormalSocket: DONE!\n";
    }

    using ExpectedSize = tl::expected<std::size_t, SocketError>;

    template <typename T>
    ExpectedSize vSend(const std::span<const T> data)
    {
        return std::visit(overload{
            [&, this](const states::SynSent     &) { return sendBuffer_.write(data); },
            [&, this](const states::SynReceived &) { return sendBuffer_.write(data); },
            [&, this](const states::Established &) { return sendBuffer_.write(data); },
            [&, this](const states::CloseWait   &) { return sendBuffer_.write(data); },
            [](const states::Closed &) -> ExpectedSize { return tl::unexpected{SocketError::CONN_NOT_EXIST}; },
            [](const auto           &) -> ExpectedSize { return tl::unexpected{SocketError::CLOSING}; }
        }, state_);
    }

    ExpectedSize vRecv(const std::span<std::byte> buff, std::size_t n)
    {
        return std::visit(overload{
            [&, this](const states::SynSent     &) -> ExpectedSize { return tl::unexpected{SocketError::NYI}; },
            [&, this](const states::SynReceived &) -> ExpectedSize { return tl::unexpected{SocketError::NYI}; },
            [&, this](const states::Established &) { return recvBuffer_.readAtMostNBytes(buff, n); },
            [&, this](const states::FinWait1    &) { return recvBuffer_.readAtMostNBytes(buff, n); },
            [&, this](const states::FinWait2    &) { return recvBuffer_.readAtMostNBytes(buff, n); },
            [&, this](const states::TimeWait    &) { return recvBuffer_.readAtMostNBytes(buff, n); },
            [&, this](const states::CloseWait   &) { return recvBuffer_.getSizeToRead()
                                                            ? recvBuffer_.readAtMostNBytes(buff, n)
                                                            : tl::unexpected{SocketError::CLOSING}; },
            [](const states::Closed &) -> ExpectedSize { return tl::unexpected{SocketError::CONN_NOT_EXIST}; },
            [](const auto           &) -> ExpectedSize { return tl::unexpected{SocketError::CLOSING}; }
        }, state_);
    }

    tl::expected<void, SocketError> vClose()
    {
        using ReturnType = tl::expected<void, SocketError>;

        return std::visit(overload{
            [this](const states::SynSent     &s) -> ReturnType {
                s.onError(SocketError::CLOSING);  // Notify caller of vConnect() that the connection is closing
                shutdown_();                      // Shut down both send & recv buffers: Threads waiting on the buffers should exit
                state_ = states::Closed{};        // Transition to CLOSED
                return {};
            },
            [this](const states::SynReceived &s) -> ReturnType {
                s.lSock.get().pendingSocks_.remove(tuple_);
                // TODO!: Wait until send buffer is emptied?
                // RFC: Queue this request until all preceding SENDs have been segmentized;
                closeAsActive_();
                return {};
            },
            [this](const states::Established &) -> ReturnType { closeAsActive_();  return {}; },
            [this](const states::CloseWait   &) -> ReturnType { closeAsPassive_(); return {}; },
            []    (const states::Closed      &) -> ReturnType { return tl::unexpected{SocketError::CONN_NOT_EXIST}; },
            []    (const auto                &) -> ReturnType { return tl::unexpected{SocketError::CLOSING}; }
        }, state_);
    }

    tl::expected<void, SocketError> vAbort()
    {
        using ReturnType = tl::expected<void, SocketError>;

        const auto retval = std::visit(overload{
            [this](const states::SynSent     &s) -> ReturnType { s.onError(SocketError::RESET); return {}; },
            [this](const states::SynReceived &s) -> ReturnType { s.lSock.get().pendingSocks_.remove(tuple_); return {}; },
            []    (const states::Closed      & ) -> ReturnType { return tl::unexpected{SocketError::CONN_NOT_EXIST}; },
            []    (const auto                & ) -> ReturnType { return {}; }
        }, state_);

        if (retval) {
            // TODO!: Flush retransmission queue
            shutdown_();
            state_ = states::Closed{};
        }

        return retval;
    }

    // Getters
    int getID() const { return id_; }
    const SessionTuple & getSessionTuple() const { return tuple_; }
    const State & getState() const { return state_; }

private:
    int id_;  // socket ID used by the kernel to identify the socket: This is NOT the same as the file descriptor
    SessionTuple tuple_;  // local and remote addresses and ports
    State state_ = states::Closed{};

    // send and recv buffers
    SendBuffer<SEND_BUFFER_SIZE> sendBuffer_;
    RecvBuffer<RECV_BUFFER_SIZE> recvBuffer_;

    // Threads for sending and retransmitting packets
    std::jthread senderThread_{&NS::senderFunction_, this};
    std::jthread zwpThread_{&NS::zwpFunction_, this};

    tns::util::threading::PeriodicThread retransmitThread_{
        RETRANSMIT_THREAD_PERIOD, &NS::retransmitFunction_, this};
    
    // Callbacks to the TCP stack (sendPacket, etc.)
    const TcpStackCallbacks tcpStackCallbacks_;

    friend WriteInfo;
    friend class TcpStack;

private:

    void shutdownSend_()
    {
        retransmitThread_.stop();
        sendBuffer_.shutdown();
    }

    void shutdownRecv_()
    {
        recvBuffer_.shutdown();
    }

    void shutdown_()
    {
        shutdownSend_();
        shutdownRecv_();
    }

    void sendFin_()
    {
        const auto seq = sendBuffer_.getNxt();
        const auto wnd = static_cast<std::uint16_t>(recvBuffer_.getSizeFree());

        std::cout << "NormalSocket::sendFin_(): seq=" << seq << ", wnd=" << wnd << "\n";

        sendBuffer_.writeAndSendOne();  // ugly hack to increment SND.NBW and SND.NXT by one
        sendPacket_(Packet::makeFinPacket(tuple_, seq, wnd));
    }

    // Send FIN, then go to FIN_WAIT_1
    void closeAsActive_()
    {
        std::cout << "NormalSocket::closeAsActive_(): Sending FIN...\n";
        // SYN_RECV/ESTABLISHED => FIN_WAIT_1
        sendFin_();
        state_ = states::FinWait1{};
        std::cout << "NormalSocket::closeAsActive_(): Transitioned to FIN_WAIT_1\n";
    }

    // Send FIN, then go to LAST_ACK
    void closeAsPassive_()
    {
        std::cout << "NormalSocket::closeAsActive_(): Sending FIN...\n";
        // CLOSE_WAIT => LAST_ACK
        sendFin_();
        state_ = states::LastAck{};
        std::cout << "NormalSocket::closeAsActive_(): Transitioned to LAST_ACK\n";
    }

    void sendPacketNoRetransmit_(const Packet &packet)
    {
        tcpStackCallbacks_.sendPacket(packet, tuple_.remote);
    }

    void sendPacket_(Packet &&packet)
    {
        // Enqueue the packet onto retransmission queue, lock must be held until the packet is sent
        const auto &[lk, entry] = sendBuffer_.retransmitQueue.enqueue(std::move(packet));
        assert(lk.owns_lock() && "NormalSocket::sendPacket_(): Lock must be held until the packet is sent");

        tcpStackCallbacks_.sendPacket(entry.get().packet, tuple_.remote);
    }

    void sendZwpPacket_(Packet &&packet)
    {
        // Enqueue the packet onto retransmission queue, lock must be held until the packet is sent
        const auto &[lk, entry] = sendBuffer_.retransmitQueue.enqueueZwp(std::move(packet));
        assert(lk.owns_lock() && "NormalSocket::sendZwpPacket_(): Lock must be held until the packet is sent");

        // Record the entry in the zwp struct
        // sendBuffer_.zwpRecordRetransmitEntry(entry.get());

        tcpStackCallbacks_.sendPacket(entry.get().packet, tuple_.remote);
    }

    void zwpFunction_()
    {
        while (true) {
            auto zwpByteViewMaybe = sendBuffer_.zwpGetProbeByte();
            if (!zwpByteViewMaybe) break;  // Socket closed

            auto &ldv = zwpByteViewMaybe.value();
            if (ldv.empty()) continue;  // No probe data needed to send

            assert(ldv.size() == 1 && "NormalSocket::zwpFunction_(): Probe data should be one byte long");
            assert(ldv.lock.owns_lock() && "NormalSocket::zwpFunction_(): Lock must be held until the ZWP packet is sent");

            // Send out probe data
            const auto &[ack, wnd] = recvBuffer_.getAckWnd();  // Locks recvBuffer_.mutex_
            sendZwpPacket_/*sendPacket_*/(Packet::makeAckPacket(
                tuple_, ldv.seq, ack, static_cast<uint16_t>(wnd),
                std::make_unique<Payload>(ldv.data.begin(), ldv.data.end())));

            std::cout << "NormalSocket::zwpFunction_(): Sent ZWP. Waiting for ACK indefinitely...\n";
            auto gotAck = sendBuffer_.zwpWaitAck(std::move(ldv));  // BLOCK on sendBuffer_.cvZwpAck_ (until ACK is received
            if (!gotAck) break;  // Socket closed

            std::cout << "Got ACK, going back to begining\n";
        }
    }

    void senderFunction_()
    {
        std::array<std::byte, MAX_TCP_PAYLOAD_SIZE> data;
        while (true) {
            // std::cout << "NormalSocket::senderFunction_(): Waiting for data to send...\n";
            const auto seqnMaybe = sendBuffer_.sendReadyData(data);  // BLOCK on sendBuffer.cvSender_
            if (!seqnMaybe) break;  // Socket closed

            const auto &[seq, n] = *seqnMaybe;
            // std::cout << "Got " << n << " new bytes to send\n";
            const auto &[ack, wnd] = recvBuffer_.getAckWnd();  // Locks recvBuffer_.mutex_

            sendPacket_(Packet::makeAckPacket(
                tuple_, seq, ack, static_cast<uint16_t>(wnd), 
                std::make_unique<Payload>(data.begin(), data.begin() + n)));
        }
    }

    void retransmitFunction_()
    {
        const auto sWndBound = sendBuffer_.getWndEndExclusive();
        const auto maybeExpEntries = sendBuffer_.retransmitQueue.getExpiredEntries(sWndBound);

        if (maybeExpEntries) {
            const auto &[lk, expEntries] = maybeExpEntries.value();
            assert((expEntries.empty() || lk.owns_lock()) && 
                "NormalSocket::retransmitFunction_(): Lock must be held until the packet is resent");
            
            for (const auto &entry : expEntries)  // Retransmit expired packets
                tcpStackCallbacks_.sendPacket(entry.get().packet, tuple_.remote);
        }
        else {
            // Max retransmits reached -> socket should be aborted
            std::cerr << "NormalSocket::retransmitFunction_(): Max #retransmits reached! Aborting socket " << id_ << "\n";
            vAbort();
        }
    }

};

} // namespace tcp 
} // namespace tns
