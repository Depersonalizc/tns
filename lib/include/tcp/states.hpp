#pragma once

#include "ip/address.hpp"
#include "tcp/session_tuple.hpp"
#include "tcp/socket_error.hpp"
#include "util/defines.hpp"
#include "util/periodic_thread.hpp"
#include "util/tl/expected.hpp"

#include <iostream>
#include <variant>
#include <string>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <span>
#include <optional>
#include <experimental/memory>

/**
 * TCP Connection States and Events for the FSM
 * As per https://en.m.wikipedia.org/wiki/File:Tcp_state_diagram_fixed_new.svg
 */

namespace tns {
namespace tcp {

// Forward declarations
class Packet;
class SessionTuple;
class NormalSocket;
class ListenSocket;
using NormalSocketRef = std::reference_wrapper<NormalSocket>;
using ListenSocketRef = std::reference_wrapper<ListenSocket>;


namespace states {

struct Closed {
    static constexpr std::string_view name{"CLOSED"};

};

struct Listen {
    static constexpr std::string_view name{"LISTEN"};

};

struct SynSent {
    static constexpr std::string_view name{"SYN_SENT"};

    // mutable std::optional<SocketError> error = std::nullopt;
    // mutable bool cvNotified = false;
    // std::shared_ptr<std::condition_variable> cv;
    // std::shared_ptr<std::mutex> mtx;

    struct SynAckResult {
        std::optional<SocketError> error = std::nullopt;
        bool notified = false;
        std::condition_variable cv;
        std::mutex mtx;

        void onError(SocketError err)
        {
            {
                std::lock_guard<std::mutex> lk(mtx);
                error = err;
                notified = true;
            }
            cv.notify_one();
        }

        void onSynAck()
        {
            {
                std::lock_guard<std::mutex> lk(mtx);
                notified = true;
            }
            cv.notify_one();
        }

        auto waitForSynAck()
        {
            std::unique_lock<std::mutex> lk(mtx);
            // The protocol handler will eventually call onSynAck() when it receives a valid SYN-ACK
            cv.wait(lk, [this] { return notified; });  // Wait indefinitely, until notified
            return error;
        }
    };
    std::experimental::observer_ptr<SynAckResult> result_;

    SynSent(SynAckResult &result) : result_{&result} {}

    ~SynSent()
    {
        if (result_) {
            result_->notified = true;
            result_->cv.notify_all();
        }
    }

    SynSent(SynSent&& other) : result_{std::exchange(other.result_, nullptr)} {}

    SynSent& operator=(SynSent&& other)
    {
        if (this != &other)
            result_ = std::exchange(other.result_, nullptr);
        return *this;
    }

    void onError(SocketError err) const
    {
        if (result_)
            result_->onError(err);
    }

    void onSynAck() const
    {
        if (result_)
            result_->onSynAck();
    }

    // auto waitForSynAck() const
    // {
    //     std::unique_lock<std::mutex> lk(result_->mtx);
    //     // The protocol handler will eventually call onSynAck() when it receives a valid SYN-ACK
    //     result_->cv.wait(lk, [this] { return result_->mtx; });  // Wait indefinitely, until notified
    //     return result_->error;
    // }

};

struct SynReceived {
    static constexpr std::string_view name{"SYN_RECEIVED"};
    ListenSocketRef lSock;  // The listener that created this socket
};

struct Established {
    static constexpr std::string_view name{"ESTABLISHED"};

};

struct FinWait1 {
    static constexpr std::string_view name{"FIN_WAIT_1"};

};

struct FinWait2 {
    static constexpr std::string_view name{"FIN_WAIT_2"};


};

struct CloseWait {
    static constexpr std::string_view name{"CLOSE_WAIT"};

};

struct Closing {
    static constexpr std::string_view name{"CLOSING"};

};

struct LastAck {
    static constexpr std::string_view name{"LAST_ACK"};

};

struct TimeWait {
    static constexpr std::string_view name{"TIME_WAIT"};

    std::chrono::steady_clock::time_point time;

    TimeWait() : time{std::chrono::steady_clock::now()} {}

    bool isExpired() const noexcept
    {
        using namespace std::chrono;
        return steady_clock::now() - time > 10s;
    }
};

using Variant = std::variant<Closed, Listen, SynSent, SynReceived, Established, 
                             FinWait1, FinWait2, CloseWait, Closing, LastAck, TimeWait>;

} // namespace states

namespace events {

struct Close {

};

struct GetSyn {
    SessionTuple session;  // Session tuple from swapping that of the SYN packet
    uint32_t clientISN;    // Initial sequence number, Host byte order
    uint16_t clientWND;    // Window size, Host byte order
};

struct GetSynAck {
    uint32_t serverISN;    // Initial sequence number, Host byte order
    uint32_t ackNum;
    uint16_t serverWND;    // Window size, Host byte order
};

struct GetAck {
    uint32_t seqNum;
    uint32_t ackNum;       // Host byte order
    uint16_t wndSize;      // Host byte order
    PayloadView payload;
};

struct GetFin {
    uint32_t seqNum;
    uint16_t wndSize;      // Host byte order
};

struct GetFinAck {
    uint32_t seqNum;
    uint32_t ackNum;       // Host byte order
    uint16_t wndSize;      // Host byte order
};

using Variant = std::variant<Close, GetSyn, GetSynAck, GetAck, GetFin, GetFinAck>;

tl::expected<Variant, std::string> fromPacket(const Packet &packet, const SessionTuple &session);

} // namespace events

using State = states::Variant;
using Event = events::Variant;

// Utility
inline std::ostream& operator<<(std::ostream& os, const State& state) {
    os << std::visit([](const auto& s) { return s.name; }, state);
    return os;
}

} // namespace tcp
} // namespace tns
