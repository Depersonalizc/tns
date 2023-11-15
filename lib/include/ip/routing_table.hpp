#pragma once

#include <map>           // map
#include <mutex>         // mutex, lock_guard
#include <shared_mutex>
#include <vector>        // vector
#include <optional>      // optional
#include <iostream>      // ostream, cout
#include <chrono>

#include "address.hpp"
#include "rip_message.hpp"
#include "util/defines.hpp"


namespace tns {

    class NetworkNode;

namespace ip {

class RoutingTable {

    friend class tns::NetworkNode;

    // passkey idiom
    struct CtorToken {};

public:
    RoutingTable(NetworkNode &node, CtorToken) : node_(node) {};

    // Move constructor
    RoutingTable(RoutingTable&& other);

    // Move assignment operator
    RoutingTable& operator=(RoutingTable&& other);

    ~RoutingTable() = default;

    enum class QueryStrategy {
        FIRST_MATCH,
        LONGEST_PREFIX_MATCH,
    };

    enum class EntryType {
        LOCAL,
        RIP,
        STATIC,
    };

    friend std::ostream& operator<< (std::ostream& os, EntryType entryType);

    struct Entry {
        EntryType type;                      // EntryType::LOCAL, ::RIP, ::STATIC
        Ipv4Address addr;                    // addr & mask defines a subnet
        in_addr_t mask;                      // Subnet mask in host byte order
        std::optional<Ipv4Address> gateway;  // If gateway is not null, requery with gateway
        NetworkInterfaceIter interfaceIt;    // Iterator into NetworkNode::interfaces_
        std::optional<std::size_t> metric;   // Metric is null for static routes
        std::chrono::steady_clock::time_point lastRefresh;  // Last time this entry was refreshed, for RIP entries
    };
    using Entries = std::vector<Entry>;

    // Query the routing table to find the entry 
    // with a matching subnet using the given strategy.
    const Entry *query_(const Ipv4Address &addr, 
                        const QueryStrategy &strategy = QueryStrategy::LONGEST_PREFIX_MATCH) const;

    // Find the first entry with a matching subnet.
    // Returns nullptr if no matching entry is found.
    const Entry *queryFirstMatch_(const Ipv4Address &addr) const;
    const Entry *queryFirstMatchNoLock_(const Ipv4Address &addr) const;

    // Find the entry with a matching subnet that has the longest prefix.
    // Returns nullptr if no matching entry is found.
    const Entry *queryLongestPrefixMatch_(const Ipv4Address &addr) const;
    const Entry *queryLongestPrefixMatchNoLock_(const Ipv4Address &addr) const;

    // Find an iterator to the exact entry in the routing table given the address and mask.
    Entries::iterator findEntry_(const Ipv4Address &addr, const std::uint32_t maskHost);
    Entries::iterator findEntryNoLock_(const Ipv4Address &addr, const std::uint32_t maskHost);

    // Write info of the routing table to an ostream. (Default is standard output.)
    void listEntries_(std::ostream &os = std::cout) const;
    void listEntriesNoLock_(std::ostream &os = std::cout) const;

    // Add an entry to the routing table.
    void addEntry_(EntryType type,
                   const std::string &cidr,
                   std::optional<Ipv4Address> gateway, 
                   NetworkInterfaceIter interfaceIt = {},
                   std::optional<std::size_t> metric = std::nullopt);

    // Enbale the local route by marking it as having 0 cost
    RipMessage enableLocalRoute_(NetworkInterfaceIter interfaceIt);
    RipMessage enableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt);

    // Mark the local route with the given interface as having infinite cost.
    RipMessage disableLocalRoute_(NetworkInterfaceIter interfaceIt);
    RipMessage disableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt);

    // ====================== RIP Related ======================
    
    bool handleRipEntryNoLock_(const RipMessage::Entry &entry, const Ipv4Address &learnedFrom);

    // Handle RIP route update. (recv)
    RipMessage handleRipEntries_(const RipMessage::Entries &ripEntries, const Ipv4Address &learnedFrom);

    // Generate RIP route entries to send to neighbors. (send)
    RipMessage generateRipEntries_() const;

    // Remove RIP routes that have been expired for RIP_EXPIRATION_TIME
    // Return the removed entries as having infinite cost for triggered update
    // Also remove RIP routes with infinite cost (poisoned routes) but don't send triggered updates for them
    constexpr RipMessage removeStaleRipEntries_(auto expirationTime);


private:
    static std::unique_ptr<RoutingTable> makeRoutingTable(NetworkNode &node);

private:
    NetworkNode &node_;
    Entries entries_;
    mutable std::shared_mutex mutex_;
};

} // namespace ip
} // namespace tns
