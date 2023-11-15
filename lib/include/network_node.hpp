#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include <mutex>
#include <optional>
#include <string_view>

#include "ip/routing_table.hpp"
#include "ip/protocols.hpp"
#include "util/defines.hpp"
#include "util/tl/expected.hpp"


namespace tns {

// Forward declaration
namespace util {

    namespace threading {
        class ThreadPool;
    } // namespace threading

    namespace lnx {
        struct NetworkNodeData;
    } // namespace lnx

} // namespace util

namespace ip {
    class Ipv4Address;
    class Datagram;
} // namespace ip

class NetworkInterface;

// Abstract class that represents either a host or a router.
class NetworkNode {
    friend NetworkInterface;
    friend ip::RoutingTable;

public:
    NetworkNode();
    NetworkNode(const std::string_view lnxFile);
    NetworkNode(const std::string &lnxFile);
    NetworkNode(NetworkNode&&) = delete;

    virtual ~NetworkNode();

    // Protocol PROTO_TEST (0): Send a test packet
    ssize_t sendIpTest(const ip::Ipv4Address &destIP, const std::string_view testMessage) const;

    // Temporarily bring down or up an interface.
    virtual void enableInterface(const std::string &name);
    virtual void disableInterface(const std::string &name);

    // Writing node info to an ostream. (Default is standard output.)
    void listInterfaces(std::ostream &os = std::cout) const;
    void listNeighbors(std::ostream &os = std::cout) const;
    void listRoutes(std::ostream &os = std::cout) const;

    // void registerRecvHandler(ip::Protocol protocol, PayloadHandler handler);
    void registerRecvHandler(ip::Protocol protocol, DatagramHandler handler);

protected:
    // Initialize the network node with the given parsed data.
    void initialize_(const util::lnx::NetworkNodeData &nodeData);

    struct QueryResult_ {
        const NetworkInterface &interface;
        const ip::Ipv4Address &nextHopAddr;
    };
    using QueryResult = tl::expected<QueryResult_, std::string>;

    // Query the routing table to find the first entry with a matching subnet.
    QueryResult queryRoutingTable_(const ip::Ipv4Address &destIP, 
                                   const ip::RoutingTable::QueryStrategy &strategy 
                                       = ip::RoutingTable::QueryStrategy::LONGEST_PREFIX_MATCH) const;

    // Returns whether the given IP address matches any of the interfaces of this network node.
    bool isMyIpAddress_(const ip::Ipv4Address &addr) const;

    // Submit the received datagram to the thread pool of the network node to process it.
    // Enqueue as a task to the thread pool.
    void submitDatagram_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const;

    // Send out a payload as an IP datagram to the given destination address using the given protocol.
    ssize_t sendIp_(const ip::Ipv4Address &destIP, PayloadPtr payload, ip::Protocol protocol) const;

    // Find an iterator to the interface with the given name.
    NetworkInterfaceIter findInterface_(const std::string &name);

    // Add an entry to the routingTable_.
    void addRoutingEntry_(ip::RoutingTable::EntryType type,
                          const std::string &cidr,
                          std::optional<ip::Ipv4Address> gateway, 
                          NetworkInterfaceIter interfaceIt,
                          std::optional<std::size_t> metric = std::nullopt);
    void addStaticRoute_(const std::string &cidr, const ip::Ipv4Address &gateway);
    void addRipRoute_(const std::string &cidr, const ip::Ipv4Address &gateway, std::size_t metric);
    void addLocalRoute_(const std::string &cidr, const ip::Ipv4Address &interfaceAddr);
    void addLocalRoute_(const std::string &cidr, NetworkInterfaceIter interfaceIt);
    
    // Invoke the handler for the protocol specified in the datagram
    void invokeProtocolHandler_(DatagramPtr datagram) const;

protected:
    // Routing table of the network node, maps dest IP addresses to interfaces
    std::unique_ptr<ip::RoutingTable> routingTable_;
 
    // Interfaces of the network node
    NetworkInterfaces interfaces_;
    std::map<ip::Ipv4Address, NetworkInterfaceIter> interfacesByAddr_;
    std::unordered_map<std::string, NetworkInterfaceIter> interfacesByName_;

    // Protocol handlers for IP datagrams
    std::unordered_map<ip::Protocol, DatagramHandler> protocolHandlers_;

    // Thread pool to handle received datagrams
    std::unique_ptr<util::threading::ThreadPool> threadPool_;

private:
    /**
     * Executed by a worker thread after a datagram arrives via one of the interfaces of this node.
     * The interface thread will call this method with the datagram as argument.
     * If this network node is a host, the datagram is delivered to the "OS".
     * If this network node is a router, the datagram is sent to the correct interface within the router. (We assume that the router doesn't have an application)
     */
    virtual void datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const = 0;
};

} // namespace tns
