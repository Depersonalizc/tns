#include "router_node.hpp"

#include "ip/datagram.hpp"
#include "ip/rip_message.hpp"
#include "network_interface.hpp"
#include "util/util.hpp"

#include "src/util/lnx_parser/parse_lnx.hpp"
#include "src/util/periodic_thread.hpp"
#include "src/util/util.hpp"

#include <sstream>
#include <iostream>


namespace tns {

using ip::Datagram, ip::Ipv4Address, ip::RipMessage;

/**
 * This constructor is called by the vrouter application to create a router node.
 * @param lnxFile The path to a .lnx file that describes the router config.
 */
RouterNode::RouterNode(const std::string_view lnxFile)
{
    ::THROW_NO_IMPL();
}

RouterNode::RouterNode() = default;
RouterNode::RouterNode(const std::string &lnxFile) : RouterNode(std::string_view(lnxFile)) {}
RouterNode::~RouterNode() { ::THROW_NO_IMPL(); }


void RouterNode::enableInterface(const std::string &name)
{
    ::THROW_NO_IMPL();
}

void RouterNode::disableInterface(const std::string &name)
{
    ::THROW_NO_IMPL();
}

/**
 * Called when a datagram arrives for this router node.
 * Assume the IP header is valid.
 * Forward the datagram to the next hop.
 */
void RouterNode::datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &/*infaceAddr*/) const
{
    ::THROW_NO_IMPL();
}

// Query the routing table to find an entry with a matching subnet.
// Then send out the datagram through the interface specified by that entry.
void RouterNode::forwardDatagram_(const Datagram &datagram) const
{
    ::THROW_NO_IMPL();
}

void RouterNode::initializeRip_()
{
    ::THROW_NO_IMPL();
}

void RouterNode::ripProtocolHandler_(DatagramPtr datagram)
{
    ::THROW_NO_IMPL();
}

void RouterNode::broadcastRipMessage_(const RipMessage &ripMessage) const
{
    ::THROW_NO_IMPL();
}

void RouterNode::sendRipMessage_(const RipMessage &ripMessage, const Ipv4Address &destIP) const
{
    ::THROW_NO_IMPL();
}

} // namespace tns
