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
    // Parse lnx file and initialize base fields
    const auto nodeData = util::lnx::parseLnx(lnxFile.data());
    NetworkNode::initialize_(nodeData);

    // Only host node does static routing
    for (const auto &route : nodeData.routes)
        addStaticRoute_(route.destAddr, route.nextHop);

    // Register TCP callback
    protocolHandlers_[ip::Protocol::TCP] = 
        [this](DatagramPtr datagram) {tcpStack_.tcpProtocolHandler(std::move(datagram));};

    // Register IP callback for the TCP stack
    tcpStack_.registerIpCallback(
        [this](const ip::Ipv4Address &destIP, PayloadPtr payload) 
            { sendIp_(destIP, std::move(payload), ip::Protocol::TCP); }
    );

    std::stringstream ss;
    ss << "/********* HostNode created with " << interfaces_.size() << " interfaces. *********/\n";
    std::cout << ss.str();
}

/**
 * This method is called when a datagram arrives for this host via one of its interfaces. 
 * If the destaddr matches the interface, let OS consume the datagram.
 * Assume the IP header is valid.
 */
void HostNode::datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const
{
    // std::cout << "HostNode::datagramHandler_(): Datagram arrived for host\n";

    if (infaceAddr == datagram->getDstAddr()) {
        // Local delivery: let OS consume the datagram
        invokeProtocolHandler_(std::move(datagram));
    } else {
        /**
         * RFC1122, Section 3.3.4.2 Multihoming Requirements
         * (A)  A host MAY silently discard an incoming datagram whose
         *      destination address does not correspond to the physical
         *      interface through which it is received.
         */
        std::stringstream ss;
        ss << "HostNode::datagramHandler_(): Discarding datagram with destination address " 
            << datagram->getDstAddr().toStringAddr() << " because it does not match the interface address\n";
        std::cout << ss.str();
    }
}

} // namespace tns
