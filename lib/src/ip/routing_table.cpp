#include "ip/routing_table.hpp"
#include "ip/util.hpp"   // sameSubnet()
#include "util/util.hpp"
#include "network_interface.hpp"

#include <algorithm>     // find_if
#include <bitset>
#include <iomanip>


namespace tns {
namespace ip {

using std::chrono::steady_clock;

const RoutingTable::Entry*
RoutingTable::query_(const Ipv4Address &addr, const QueryStrategy &strategy) const
{
    switch (strategy) {
        case QueryStrategy::FIRST_MATCH:
            return queryFirstMatch_(addr);
        case QueryStrategy::LONGEST_PREFIX_MATCH:
            return queryLongestPrefixMatch_(addr);
        default:
            return nullptr;
    }
}

const RoutingTable::Entry*
RoutingTable::queryFirstMatch_(const Ipv4Address &addr) const
{
    std::shared_lock lock(mutex_);
    return queryFirstMatchNoLock_(addr);
}

const RoutingTable::Entry*
RoutingTable::queryFirstMatchNoLock_(const Ipv4Address &addr) const
{
    auto it = std::find_if(entries_.cbegin(), entries_.cend(),
        [&addr](const auto &entry) {
            return util::sameSubnet(addr, entry.addr, entry.mask);
        });
    return it == entries_.cend() ? nullptr : &*it;
}

const RoutingTable::Entry*
RoutingTable::queryLongestPrefixMatch_(const Ipv4Address &addr) const
{
    std::shared_lock lock(mutex_);
    return queryLongestPrefixMatchNoLock_(addr);
}

// Iterate over all matching entries and return the one with the longest prefix.
const RoutingTable::Entry*
RoutingTable::queryLongestPrefixMatchNoLock_(const Ipv4Address &addr) const
{
    const Entry *longestPrefixEntry = nullptr;
    for (const auto &entry : entries_) {
        if (util::sameSubnet(addr, entry.addr, entry.mask)) {
            if (!longestPrefixEntry || entry.mask > longestPrefixEntry->mask)
                longestPrefixEntry = &entry;
        }
    }
    return longestPrefixEntry;
}

// Find an iterator to the exact entry in the routing table given the address and mask.
RoutingTable::Entries::iterator
RoutingTable::findEntry_(const Ipv4Address &addr, in_addr_t maskHost)
{
    std::shared_lock lock(mutex_);
    return findEntryNoLock_(addr, maskHost);
}

RoutingTable::Entries::iterator
RoutingTable::findEntryNoLock_(const Ipv4Address &addr, in_addr_t maskHost)
{
    return std::find_if(entries_.begin(), entries_.end(),
        [&addr, maskHost](auto &entry) {
            return (entry.addr.getAddrHost() & entry.mask) 
                      == (addr.getAddrHost() & maskHost);
        });
}


// Lock the routing table and add a new entry.
void RoutingTable::addEntry_(EntryType type, const std::string &cidr, 
                             std::optional<Ipv4Address> gateway, 
                             NetworkInterfaceIter interfaceIt, 
                             std::optional<std::size_t> metric)
{
    // std::cout << "RoutingTable::addEntry_(): cidr = " << cidr << ", gateway = " << gateway << "\n";
    auto subnet = util::parseCidr(cidr);
    if (!subnet) {
        std::cerr << "RoutingTable::addEntry_(): " << subnet.error();
    } else {
        auto [addr, mask, _] = subnet.value();
        std::unique_lock lock(mutex_);
        entries_.emplace_back(type, addr, mask, gateway, interfaceIt, metric, steady_clock::now());
    }
}

void RoutingTable::listEntries_(std::ostream &os) const
{
    std::shared_lock lock(mutex_);
    listEntriesNoLock_(os);
}

void RoutingTable::listEntriesNoLock_(std::ostream &os) const
{
    using namespace std;

    os << setw(2)  << left  << "T"        << " "
       << setw(18) << left  << "Prefix"   << " "
       << setw(15) << left  << "Next hop" << " "
       << setw(5)  << right << "Cost"     << "\n";

    for (const auto &entry : entries_) {
        os << setw(2)  << left  << (entry.type) << " "
           << setw(18) << left  << (entry.addr.toStringAddr() + "/" + to_string(util::subnetMaskLength(entry.mask))) << " "
           << setw(15) << left  << (entry.gateway ? entry.gateway.value().toStringAddr() 
                                                  : string("LOCAL:") + entry.interfaceIt->name_) << " "
           << setw(5)  << right << (entry.metric ? to_string(entry.metric.value()) : "-") << "\n";
    }
}

RipMessage RoutingTable::enableLocalRoute_(NetworkInterfaceIter interfaceIt)
{
    std::unique_lock lock(mutex_);
    return enableLocalRouteNoLock_(interfaceIt);
}

RipMessage RoutingTable::enableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt)
{
    RipMessage::Entries updatedEntry;
    RipMessage::OptionalAddresses learnedFrom;

    for (auto it = entries_.begin(); it != entries_.end(); ) {
        if (it->type == EntryType::LOCAL && it->interfaceIt == interfaceIt) {
            // it->metric = 0;
            updatedEntry.emplace_back(0, it->addr.getAddrHost(), it->mask);
            learnedFrom.push_back(it->gateway);  //  nullopt
            break;  // At most one local route per interface
        } else {
            it++;
        }
    }

    return RipMessage::makeResponse(std::move(updatedEntry), std::move(learnedFrom));
}

RipMessage RoutingTable::disableLocalRoute_(NetworkInterfaceIter interfaceIt)
{
    std::unique_lock lock(mutex_);
    return disableLocalRouteNoLock_(interfaceIt);
}

RipMessage RoutingTable::disableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt)
{
    RipMessage::Entries updatedEntry;
    RipMessage::OptionalAddresses learnedFrom;

    for (auto it = entries_.begin(); it != entries_.end(); ) {
        if (it->type == EntryType::LOCAL && it->interfaceIt == interfaceIt) {
            // it->metric = RipMessage::INFINITY;
            updatedEntry.emplace_back(RipMessage::INFINITY, it->addr.getAddrHost(), it->mask);
            learnedFrom.push_back(it->gateway);  //  nullopt
            break;  // At most one local route per interface
        } else {
            it++;
        }
    }

    return RipMessage::makeResponse(std::move(updatedEntry), std::move(learnedFrom));
}

/*********************** RIP Related ***********************/
RipMessage RoutingTable::generateRipEntries_() const
{
    RipMessage::Entries ripEntries;
    RipMessage::OptionalAddresses learnedFrom;

    std::shared_lock lock(mutex_);
    for (const auto &entry : entries_) {
        ripEntries.emplace_back(
            entry.metric.value(),        // metric
            entry.addr.getAddrHost(),  // addr
            entry.mask                   // mask
        );
        learnedFrom.push_back(entry.gateway);
    }

    return RipMessage::makeResponse(std::move(ripEntries), std::move(learnedFrom));
}

// This is called when a router node receives a RIP response
RipMessage RoutingTable::handleRipEntries_(const RipMessage::Entries &ripEntries, 
                                           const Ipv4Address &learnedFrom)
{
    RipMessage::Entries updatedEntries;
    RipMessage::OptionalAddresses learnedFroms;

    std::unique_lock lock(mutex_);
    for (const auto &ripEntry : ripEntries) {
        // Find an existing entry with the same subnet
        Ipv4Address ripEntryAddr{ tns::util::hton(ripEntry.address) };
        auto it = findEntryNoLock_(ripEntryAddr, ripEntry.mask);  // Mutex already locked by handleRipEntries_()
        
        if (it != entries_.end()) {
            // Skip if a local route already exists
            if (it->type == EntryType::LOCAL)
                continue;

            // Update the existing entry if the new metric is smaller, or,
            // if the new metric is greater but the entry is learned from the same neighbor
            if (ripEntry.cost < it->metric) {
                it->lastRefresh = steady_clock::now();
                it->metric = ripEntry.cost;
                it->gateway = learnedFrom;
                std::cout << "existing entry, update to smaller metric\n";
            }
            else if (it->gateway == learnedFrom) {
                // Refresh the entry regardless of the metric
                it->lastRefresh = steady_clock::now();
                // If same cost, don't send triggered update
                if (ripEntry.cost == it->metric)
                    continue;
                // Update to greater metric
                it->metric = ripEntry.cost;
                std::cout << "existing entry, update to greater metric\n";
            }
            else {
                continue;
            }
        }
        else if (ripEntry.cost < RipMessage::INFINITY) {
            std::cout << "new non-poison entry\n";
            entries_.emplace_back(EntryType::RIP, ripEntryAddr, ripEntry.mask, learnedFrom, 
                                  node_.interfaces_.end(), ripEntry.cost, steady_clock::now());
        }
        else {
            continue;
        }

        // Collect triggered update
        updatedEntries.emplace_back(ripEntry.cost, ripEntry.address, ripEntry.mask);
        learnedFroms.push_back(learnedFrom);
    }

    return RipMessage::makeResponse(std::move(updatedEntries), std::move(learnedFroms));
}

} // namespace ip
} // namespace tns
