#pragma once

#include "network_node.hpp"

#include <mutex>
#include <condition_variable>

namespace tns {

// This network node represents a router.
class RouterNode : public NetworkNode {
public:
    RouterNode();
    RouterNode(const std::string_view lnxFile);
    RouterNode(const std::string &lnxFile);
    RouterNode(const RouterNode&) = delete;
    RouterNode(RouterNode&&) = delete;

    ~RouterNode();

    void  enableInterface(const std::string &name) override;
    void disableInterface(const std::string &name) override;

private:
    void datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const override;

    // Forward the received datagram to the correct interface within the network node.
    void forwardDatagram_(const ip::Datagram &datagram) const;

    // Spin up RIP related threads
    void initializeRip_();

    // Handler upon receiving a RIP packet
    void ripProtocolHandler_(DatagramPtr datagram);

    // Helpers for sending a RIP message to one/all RIP neighbors
    // NOTE: These increment the metric of the RIP routes by 1
    void sendRipMessage_(const ip::RipMessage &ripMessage, const ip::Ipv4Address &destIP) const;
    void broadcastRipMessage_(const ip::RipMessage &ripMessage) const;

private:
    std::vector<ip::Ipv4Address> ripNeighbors_;

    // Thread for sending RIP packets to all RIP neighbors at a constant rate
    PeriodicThreadPtr ripThread_;
    constexpr static std::chrono::duration RIP_INTERVAL = std::chrono::seconds(5);  // Send RIP packets every 5 seconds

    // Thread for cleaning up RIP routes if they are not refreshed for a certain amount of time
    PeriodicThreadPtr ripCleanerThread_;
    constexpr static std::chrono::duration RIP_CLEANER_INTERVAL = std::chrono::milliseconds(500);  // Clean up RIP routes every half a second
    constexpr static std::chrono::duration RIP_EXPIRATION_TIME = std::chrono::seconds(12);  // Expire RIP routes not refreshed for 12 seconds
};

} // namespace tns
