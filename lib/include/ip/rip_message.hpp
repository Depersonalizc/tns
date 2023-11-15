#pragma once

#include "util/defines.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <optional>
#include <vector>

namespace tns {
namespace ip {

class Ipv4Address;

// Virtual IPv4 address defined in a lnx file
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

    static RipMessage makeRequest();
    static RipMessage makeResponse(Entries entries, OptionalAddresses learnedFrom);

    RipMessage(PayloadView payload);  // recv, convert to host byte order when receiving
    ~RipMessage();

    Command getCommand() const;
    std::uint16_t getNumEntries() const;
    const Entries &getEntries() const;
    const OptionalAddresses &getLearnedFrom() const;
    std::size_t payloadSize() const;

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
