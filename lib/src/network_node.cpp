#include "network_node.hpp"

#include <cassert>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>  // std::any_of
#include <stdexcept>  // std::runtime_error

#include "ip/util.hpp"                       // parseCidr()
#include "ip/datagram.hpp"
#include "ip/protocols.hpp"
#include "network_interface.hpp"
#include "src/util/thread_pool.hpp"
#include "src/util/lnx_parser/parse_lnx.hpp"


namespace tns {

using ip::Ipv4Address, ip::RoutingTable, ip::Datagram;

/***** Public *****/
NetworkNode::NetworkNode(const std::string_view lnxFile)
{
    ::THROW_NO_IMPL();
}

NetworkNode::NetworkNode() = default;
NetworkNode::NetworkNode(const std::string &lnxFile) : NetworkNode(std::string_view(lnxFile)) {}
NetworkNode::~NetworkNode() { ::THROW_NO_IMPL(); }

ssize_t NetworkNode::sendIpTest(const Ipv4Address &destIP, const std::string_view testMessage) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::enableInterface(const std::string &name)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::disableInterface(const std::string &name)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::listInterfaces(std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::listNeighbors(std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::listRoutes(std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::registerRecvHandler(ip::Protocol protocol, DatagramHandler handler)
{
    ::THROW_NO_IMPL();
}


/***** Protected & Private *****/

// Initialize the network node with the given parsed nodeData returned by util::lnx::parseLnx().
void NetworkNode::initialize_(const util::lnx::NetworkNodeData &nodeData)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::invokeProtocolHandler_(DatagramPtr datagram) const
{
    ::THROW_NO_IMPL();
}

bool NetworkNode::isMyIpAddress_(const Ipv4Address &addr) const
{
    ::THROW_NO_IMPL();
}

NetworkInterfaceIter NetworkNode::findInterface_(const std::string &name)
{
    ::THROW_NO_IMPL();
}

ssize_t NetworkNode::sendIp_(const Ipv4Address &destIP, PayloadPtr payload, ip::Protocol protocol) const
{
    ::THROW_NO_IMPL();
}

// Find the final entry in the routing table that matches the given destination address.
// Returns an optional pair of interface and the next hop IP address.
// This potentially involves 2 lookups if the gateway is not null.
NetworkNode::QueryResult
NetworkNode::queryRoutingTable_(const Ipv4Address &destIP, const RoutingTable::QueryStrategy &strategy) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::submitDatagram_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const
{
    ::THROW_NO_IMPL();
}

void NetworkNode::addRoutingEntry_(RoutingTable::EntryType entryType,
                                   const std::string &cidr, std::optional<Ipv4Address> gateway,
                                   NetworkInterfaceIter interfaceIt, std::optional<std::size_t> metric)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::addStaticRoute_(const std::string &cidr, const Ipv4Address &gateway)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::addRipRoute_(const std::string &cidr, const Ipv4Address &gateway, std::size_t metric)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::addLocalRoute_(const std::string &cidr, const Ipv4Address &interfaceAddr)
{
    ::THROW_NO_IMPL();
}

void NetworkNode::addLocalRoute_(const std::string &cidr, NetworkInterfaceIter interfaceIt)
{
    ::THROW_NO_IMPL();
}

} // namespace tns
