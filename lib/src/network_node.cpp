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
    std::cout << "NetworkNode::NetworkNode(): Constructing ...\n";
    initialize_( util::lnx::parseLnx(lnxFile.data()) );
    std::cout << "NetworkNode::NetworkNode(): DONE\n";
}

NetworkNode::NetworkNode() = default;
NetworkNode::NetworkNode(const std::string &lnxFile) : NetworkNode(std::string_view(lnxFile)) {}
NetworkNode::~NetworkNode() { std::cout << "NetworkNode::~NetworkNode(): Destructing ...\n"; }

ssize_t NetworkNode::sendIpTest(const Ipv4Address &destIP, const std::string_view testMessage) const
{
    // std::cout << "NetworkNode::sendIpTest(): Sending test message to " << destIP << "\n";
    
    PayloadPtr payload = std::make_unique<Payload>();
    payload->reserve(testMessage.size());
    std::transform(testMessage.cbegin(), testMessage.cend(), std::back_inserter(*payload),
                   [](const char &c) { return static_cast<std::byte>(c); });
    return sendIp_(destIP, std::move(payload), ip::Protocol::TEST);
}

void NetworkNode::enableInterface(const std::string &name)
{
    const auto iface = findInterface_(name);
    if (iface == interfaces_.end()) {
        std::stringstream ss;
        ss << "NetworkNode::enableInterface(): Interface named " << name << " not found\n";
        std::cerr << ss.str();
    }

    if (iface->isOff()) {
        iface->turnOn();
        routingTable_->enableLocalRoute_(iface);
    }
}

void NetworkNode::disableInterface(const std::string &name)
{
    const auto iface = findInterface_(name);
    if (iface == interfaces_.end()) {
        std::stringstream ss;
        ss << "NetworkNode::disableInterface(): Interface named " << name << " not found\n";
        std::cerr << ss.str();
    }

    if (iface->isOn()) {
        iface->turnOff();
        routingTable_->disableLocalRoute_(iface);
    }
}

void NetworkNode::listInterfaces(std::ostream &os) const
{
    using namespace std;
    os << setw(10) << left  << "Name"        << " "
       << setw(18) << left  << "Addr/Prefix" << " "
       << setw(5)  << right << "State"       << "\n";

    std::stringstream cidr;
    for (const auto &iface : interfaces_) {
        cidr.str("");
        cidr << iface.ipAddress_.toStringAddr() << "/" << iface.subnetMaskLength_;
        os << setw(10) << left  << iface.name_ << " "
           << setw(18) << left  << cidr.str() << " "
           << setw(5)  << right << (iface.isUp_ ? "up" : "down") << "\n";
    }
}

void NetworkNode::listNeighbors(std::ostream &os) const
{
    using namespace std;
    os << setw(9)  << left  << "Iface"   << " "
       << setw(15) << left  << "VIP"     << " "
       << setw(21) << right << "UDPAddr" << "\n";

    std::stringstream udp;
    for (const auto &iface : interfaces_) {
        if (!iface.isUp_) 
            continue;
        for (const auto &neighbor : iface.neighborInterfaces_) {
            udp.str("");
            udp << neighbor.udpAddr_.toStringAddr() << ":" << neighbor.udpPort_;
            os << setw(9)  << left  << iface.name_ << " "
               << setw(15) << left  << neighbor.ipAddress_.toStringAddr() << " "
               << setw(21) << right << udp.str() << "\n";
        }
    }
}

void NetworkNode::listRoutes(std::ostream &os) const
{
    routingTable_->listEntries_(os);
}

void NetworkNode::registerRecvHandler(ip::Protocol protocol, DatagramHandler handler)
{
    if (protocol == ip::Protocol::RIP) {
        std::cerr << "NetworkNode::registerRecvHandler(): ERROR: Cannot register handler for RIP protocol (200)\n";
        return;
    }
    protocolHandlers_[protocol] = handler;
}


/***** Protected & Private *****/

// Initialize the network node with the given parsed nodeData returned by util::lnx::parseLnx().
void NetworkNode::initialize_(const util::lnx::NetworkNodeData &nodeData)
{
    // Create thread pool
    threadPool_ = std::make_unique<util::threading::ThreadPool>(8);

    // Create routing table
    routingTable_ = RoutingTable::makeRoutingTable(*this);

    // Create interfaces
    interfaces_.reserve(nodeData.interfaces.size());     // Important: ensure that interfaces_ does not reallocate
    auto ifaceIt = interfaces_.end();                    // so that iterators to its elements remain valid

    for (const auto &ifaceData : nodeData.interfaces) {
        // Add interface to interfaces_
        NetworkInterface iface {
            [this](DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) {
                submitDatagram_(std::move(datagram), infaceAddr);
            },
            ifaceData.cidr, ifaceData.ip_addrs, ifaceData.udp_ports,
            ifaceData.udp_addrs, ifaceData.udp_port, ifaceData.name
        };

        interfaces_.emplace_back(std::move(iface));
        ifaceIt->startListening();  // Start listening on the interface

        // Add interface address
        if ((interfacesByAddr_.emplace(ifaceIt->ipAddress_, ifaceIt)).second == false) {
            std::stringstream ss;
            ss << "NetworkNode::NetworkNode(): Found duplicate interface IP address: " << ifaceData.cidr;
            throw std::runtime_error(ss.str());
        }

        // Add interface name
        if ((interfacesByName_.emplace(ifaceIt->name_, ifaceIt)).second == false) {
            std::stringstream ss;
            ss << "NetworkNode::NetworkNode(): Found duplicate interface name: " << ifaceData.name;
            throw std::runtime_error(ss.str());
        }

        // Add interface's subnet to the routing table as a local route
        addLocalRoute_(ifaceData.cidr, ifaceIt);

        ifaceIt++;
    }

    // Register Recv handlers
    protocolHandlers_[ip::Protocol::TEST] = ip::testProtocolHandler;
}

void NetworkNode::invokeProtocolHandler_(DatagramPtr datagram) const
{
    const auto &protocol = datagram->getProtocol();
    const auto it = protocolHandlers_.find(protocol);
    if (it != protocolHandlers_.end()) {
        // std::cout << "NetworkNode::protocolHandler_(): Handling datagram with protocol " 
        //           << static_cast<int>(protocol) << "\n";
        it->second(std::move(datagram));  // Invoke the protocol handler
    } else {
        std::stringstream ss;
        ss << "NetworkNode::protocolHandler_(): ERROR: No handler for protocol " 
           << static_cast<int>(protocol) << "\n";
        std::cerr << ss.str();
    }
}

bool NetworkNode::isMyIpAddress_(const Ipv4Address &addr) const
{
    return interfacesByAddr_.contains(addr);
}

NetworkInterfaceIter NetworkNode::findInterface_(const std::string &name)
{
    auto it = interfacesByName_.find(name);
    if (it == interfacesByName_.end())
        return interfaces_.end();
    return it->second;
}

ssize_t NetworkNode::sendIp_(const Ipv4Address &destIP, PayloadPtr payload, ip::Protocol protocol) const
{
    size_t payloadSize = payload->size();
    
    if (isMyIpAddress_(destIP)) {
        // std::cout << "NetworkNode::sendIp_(): Destination IP address is my own - Handle locally.\n";
        invokeProtocolHandler_(
            std::make_unique<Datagram>(Ipv4Address::LOCALHOST(), destIP, std::move(payload), protocol)
        );
    } else {
        // Query the routing table to find the interface to send the datagram to.
        const auto nextHop = queryRoutingTable_(destIP);
        if (!nextHop) {
            std::cerr << "NetworkNode::sendIp_(): " << nextHop.error() << "\n";
            return -1;
        }
        // Send the datagram out through that interface.
        const auto &[outInterface, nextHopAddr] = nextHop.value();
        const auto &sourceIP = outInterface.ipAddress_;
        Datagram datagram(sourceIP, destIP, std::move(payload), protocol);
        // {
        //     std::stringstream ss;
        //     ss << "NetworkNode::sendIp_(): Sending datagram to " << destIP 
        //        << " through interface " << outInterface.name_ 
        //        << " with next hop " << nextHopAddr << "\n";
        //     std::cout << ss.str();
        // }
        outInterface.sendDatagram(datagram, nextHopAddr);
    }

    // For now, assume that send() calls always succeed.
    return payloadSize;
}

// Find the final entry in the routing table that matches the given destination address.
// Returns an optional pair of interface and the next hop IP address.
// This potentially involves 2 lookups if the gateway is not null.
NetworkNode::QueryResult
NetworkNode::queryRoutingTable_(const Ipv4Address &destIP, const RoutingTable::QueryStrategy &strategy) const
{
    // Query the routing table to find the interface to send the datagram to.
    auto entry = routingTable_->query_(destIP, strategy);

    if (!entry)
        return tl::unexpected("Unreachable destination " + destIP.toStringAddr());

    // If the gateway is not nullopt, find the interface to that gateway
    if (entry->gateway) {
        const auto &gateway = entry->gateway.value();
        if ((entry = routingTable_->query_(gateway, strategy)) == nullptr)
            return tl::unexpected("Unreachable gateway " + gateway.toStringAddr());
        return QueryResult({
            *entry->interfaceIt,
            gateway
        });
    }

    // Otherwise, destIP is on link.
    return QueryResult({
        *entry->interfaceIt,
        destIP
    });
}

void NetworkNode::submitDatagram_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const
{
    std::packaged_task<void()> task{
        [this, d = std::move(datagram), &infaceAddr]() mutable {
            datagramHandler_(std::move(d), infaceAddr);
    }};
    threadPool_->enqueueTask(std::move(task));
}

void NetworkNode::addRoutingEntry_(RoutingTable::EntryType entryType,
                                   const std::string &cidr, std::optional<Ipv4Address> gateway,
                                   NetworkInterfaceIter interfaceIt, std::optional<std::size_t> metric)
{
    routingTable_->addEntry_(entryType, cidr, gateway, interfaceIt, metric);
}

void NetworkNode::addStaticRoute_(const std::string &cidr, const Ipv4Address &gateway)
{
    addRoutingEntry_(RoutingTable::EntryType::STATIC, cidr, gateway, interfaces_.end());
}

void NetworkNode::addRipRoute_(const std::string &cidr, const Ipv4Address &gateway, std::size_t metric)
{
    addRoutingEntry_(RoutingTable::EntryType::RIP, cidr,
                     gateway, interfaces_.end(), metric);
}

void NetworkNode::addLocalRoute_(const std::string &cidr, const Ipv4Address &interfaceAddr)
{
    const auto it = interfacesByAddr_.find(interfaceAddr);
    if (it == interfacesByAddr_.end()) {
        std::stringstream ss;
        ss << "NetworkNode::addLocalRoute_(): Interface with address " << interfaceAddr.toStringAddr() << " not found\n";
        std::cerr << ss.str();
    } else {
        addLocalRoute_(cidr, it->second);
    }
}

void NetworkNode::addLocalRoute_(const std::string &cidr, NetworkInterfaceIter interfaceIt)
{
    addRoutingEntry_(RoutingTable::EntryType::LOCAL, cidr, std::nullopt, interfaceIt, 0UL);
}

} // namespace tns
