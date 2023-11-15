#pragma once

#include "ip/address.hpp"
#include "tcp/session_tuple.hpp"
#include "util/tl/expected.hpp"
#include "util/defines.hpp"

#include <iostream>
#include <variant>
#include <string>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <span>

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
    static constexpr auto TIMEOUT = std::chrono::seconds{10};  // timeout after 5 seconds

    mutable bool recvedSynAck = false;
    std::unique_ptr<std::condition_variable> cv;
    std::unique_ptr<std::mutex> mtx;

    SynSent();

    auto waitForSynAck() const;
    void onSynAck() const;
};

struct SynReceived {
    static constexpr std::string_view name{"SYN_RECEIVED"};
    ListenSocketRef lSock;  // The listener that created this socket
};

struct Established {
    static constexpr std::string_view name{"ESTABLISHED"};

    // MAYBE?
    std::thread senderThread;
};

struct FinWait1 {
    static constexpr std::string_view name{"FIN_WAIT_1"};

};

struct FinWait2 {
    static constexpr std::string_view name{"FIN_WAIT_2"};

};

struct CloseWait {
    static constexpr std::string_view name{"CLOSE_WAIT"};

    // Moved from Established
    std::thread senderThread;
};

struct Closing {
    static constexpr std::string_view name{"CLOSING"};

};

struct LastAck {
    static constexpr std::string_view name{"LAST_ACK"};

};

struct TimeWait {
    static constexpr std::string_view name{"TIME_WAIT"};

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
    uint16_t clientWDN;    // Window size, Host byte order
};

struct GetAck {
    uint32_t seqNum;
    uint32_t ackNum;       // Host byte order
    uint16_t wndSize;      // Host byte order
    PayloadView payload;
};

struct GetSynAck {
    uint32_t serverISN;    // Initial sequence number, Host byte order
    uint32_t ackNum;
    uint16_t serverWDN;    // Window size, Host byte order
};

struct GetRst {
    // ip::Ipv4Address from;  // From where?
};

using Variant = std::variant<Close, GetSyn, GetRst, GetAck, GetSynAck>;

tl::expected<Variant, std::string> fromPacket(const Packet &packet, const SessionTuple &session);

} // namespace events

using State = states::Variant;
using Event = events::Variant;

// Utility
inline std::ostream& operator<<(std::ostream& os, const State& state);

// // Forward declared visitors for Event & State
// struct EventHandler;

} // namespace tcp
} // namespace tns
