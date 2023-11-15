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
    ::THROW_NO_IMPL();
}

const RoutingTable::Entry*
RoutingTable::queryFirstMatch_(const Ipv4Address &addr) const
{
    ::THROW_NO_IMPL();
}

const RoutingTable::Entry*
RoutingTable::queryFirstMatchNoLock_(const Ipv4Address &addr) const
{
    ::THROW_NO_IMPL();
}

const RoutingTable::Entry*
RoutingTable::queryLongestPrefixMatch_(const Ipv4Address &addr) const
{
    ::THROW_NO_IMPL();
}

// Iterate over all matching entries and return the one with the longest prefix.
const RoutingTable::Entry*
RoutingTable::queryLongestPrefixMatchNoLock_(const Ipv4Address &addr) const
{
    ::THROW_NO_IMPL();
}

// Find an iterator to the exact entry in the routing table given the address and mask.
RoutingTable::Entries::iterator
RoutingTable::findEntry_(const Ipv4Address &addr, in_addr_t maskHost)
{
    ::THROW_NO_IMPL();
}

RoutingTable::Entries::iterator
RoutingTable::findEntryNoLock_(const Ipv4Address &addr, in_addr_t maskHost)
{
    ::THROW_NO_IMPL();
}


// Lock the routing table and add a new entry.
void RoutingTable::addEntry_(EntryType type, const std::string &cidr, 
                             std::optional<Ipv4Address> gateway, 
                             NetworkInterfaceIter interfaceIt, 
                             std::optional<std::size_t> metric)
{
    ::THROW_NO_IMPL();
}

void RoutingTable::listEntries_(std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

void RoutingTable::listEntriesNoLock_(std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

RipMessage RoutingTable::enableLocalRoute_(NetworkInterfaceIter interfaceIt)
{
    ::THROW_NO_IMPL();
}

RipMessage RoutingTable::enableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt)
{
    ::THROW_NO_IMPL();
}

RipMessage RoutingTable::disableLocalRoute_(NetworkInterfaceIter interfaceIt)
{
    ::THROW_NO_IMPL();
}

RipMessage RoutingTable::disableLocalRouteNoLock_(NetworkInterfaceIter interfaceIt)
{
    ::THROW_NO_IMPL();
}

/*********************** RIP Related ***********************/
RipMessage RoutingTable::generateRipEntries_() const
{
    ::THROW_NO_IMPL();
}

// This is called when a router node receives a RIP response
RipMessage RoutingTable::handleRipEntries_(const RipMessage::Entries &ripEntries, 
                                           const Ipv4Address &learnedFrom)
{
    ::THROW_NO_IMPL();
}

} // namespace ip
} // namespace tns
