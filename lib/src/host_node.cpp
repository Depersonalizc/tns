#include "host_node.hpp"

#include <sstream>
#include <iostream>
#include <variant>

#include "ip/datagram.hpp"
#include "network_interface.hpp"
#include "src/util/lnx_parser/parse_lnx.hpp"


namespace tns {

/**
 * This constructor is called by the vhost application to create a host node.
 * @param lnxFile The path to a .lnx file that describes the host config.
 */
HostNode::HostNode(const std::string_view lnxFile)
{
    ::THROW_NO_IMPL();
}

/**
 * This method is called when a datagram arrives for this host via one of its interfaces. 
 * If the destaddr matches the interface, let OS consume the datagram.
 * Assume the IP header is valid.
 */
void HostNode::datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const
{
    ::THROW_NO_IMPL();
}

} // namespace tns
