#pragma once

#include "util/defines.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <optional>
#include <vector>

namespace tns {
namespace ip {

class Ipv4Address;

// Routing Information Protocol (RIP) packet
class RipMessage {
public:
    static constexpr std::uint32_t INFINITY = 16;

    enum class Command : std::uint16_t {
        REQUEST = 1,
        RESPONSE = 2,
    };

    struct Entry {  // Host byte order
        std::uint32_t cost;  
        std::uint32_t address;
        std::uint32_t mask;
    };
    using Entries = std::vector<Entry>;

    using OptionalAddresses = std::vector<std::optional<Ipv4Address>>;

    static RipMessage makeRequest() {
        return RipMessage(Command::REQUEST, {}, {});
    }
    static RipMessage makeResponse(Entries entries, OptionalAddresses learnedFrom) {
        return RipMessage(Command::RESPONSE, std::move(entries), std::move(learnedFrom));
    }

    RipMessage(PayloadView payload);  // recv, convert to host byte order when receiving
    ~RipMessage() { /* std::cout << "RipMessage::~RipMessage() : DONE!\n"; */ }

    Command getCommand() const { return command_; }
    std::uint16_t getNumEntries() const { return numEntries_; }
    const Entries &getEntries() const { return entries_; }
    const OptionalAddresses &getLearnedFrom() const { return learnedFrom_; }
    std::size_t payloadSize() const { return sizeof(command_) + sizeof(numEntries_) + entries_.size() * sizeof(Entry); }

private:
    RipMessage(Command command, Entries entries, OptionalAddresses learnedFrom);  // send, convert to network byte order when sending

    // RIP packet payload: Host byte order
    Command command_;
    std::uint16_t numEntries_;
    Entries entries_;

    // Optionally used when the router generates a RipMessage.
    // When broadcasting, the router should poison entries_[i] 
    // if the neighbor receiver matches learnedFrom_[i].
    OptionalAddresses learnedFrom_;
};

} // namespace ip
} // namespace tns
