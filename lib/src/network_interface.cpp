#include "network_interface.hpp"
#include "ip/datagram.hpp"
#include "ip/util.hpp"
#include "src/util/util.hpp"
#include "util/util.hpp"

#include <cassert>       // assert()
#include <cstring>       // strerror()
#include <unistd.h>      // close()
#include <sstream>       // std::stringstream
#include <algorithm>     // std::find_if()
#include <stdexcept>     // std::runtime_error



namespace tns {

NetworkInterface::NetworkInterface(NodeDatagramSubmitter submitter,
                                   const std::string &cidr,
                                   const std::vector<std::string> &neighborIpAddrs,
                                   const std::vector<in_port_t> &neighborUdpPorts,  // host byte order
                                   const std::vector<std::string> &neighborUdpAddrs,
                                   in_port_t udpPort,  // host byte order
                                   std::string name)
    : name_(std::move(name))
{
    ::THROW_NO_IMPL();
}

NetworkInterface::~NetworkInterface() noexcept
{
    ::THROW_NO_IMPL();
}

NetworkInterface::NetworkInterface(NetworkInterface&& other) noexcept :
    ipAddress_(other.ipAddress_),
    subnetMask_(other.subnetMask_),
    subnetMaskLength_(other.subnetMaskLength_),
    neighborInterfaces_(std::move(other.neighborInterfaces_)),
    udp_sock_(std::exchange(other.udp_sock_, -1)),
    recvThread_(std::move(other.recvThread_)),
    name_(std::move(other.name_)),
    isUp_(other.isUp_),
    datagramSubmitter_(other.datagramSubmitter_)
{
    ::THROW_NO_IMPL();
}

bool NetworkInterface::operator==(const ip::Ipv4Address& addr) const { ::THROW_NO_IMPL(); }
bool NetworkInterface::operator!=(const ip::Ipv4Address& addr) const { ::THROW_NO_IMPL(); }

void NetworkInterface::startListening()
{
    ::THROW_NO_IMPL();
}

// Receive a single datagram from udp_sock_ and submit it to the network node
void NetworkInterface::recvDatagram() const
{
    ::THROW_NO_IMPL();
}

/**
 * This method is called when a datagram is presented to this interface from 
 * the router node, i.e., the router will call interface.sendDatagram(datagram).
 * Send the datagram to the correct next-hop interface in neighborInterfaces_.
 */
void NetworkInterface::sendDatagram(const ip::Datagram &datagram, const ip::Ipv4Address &nextHopAddr) const
{
    ::THROW_NO_IMPL();
}

/* Returns the next-hop interface (in neighborInterfaces_) reachable from this interface */
NetworkInterface::InterfaceEntries::const_iterator
NetworkInterface::findNextHopInterface(const ip::Ipv4Address &nextHopAddr) const
{
    ::THROW_NO_IMPL();
}

} // namespace tns
