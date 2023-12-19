#pragma once

#include <vector>
#include <thread>
#include <chrono>

#include "network_node.hpp"
#include "ip/address.hpp"


namespace tns {

// Network interface of a node
class NetworkInterface {

    friend class NetworkNode;
    friend class HostNode;
    friend class ip::RoutingTable;

// Visible to NetworkNode and derived classes
public:
    NetworkInterface(NetworkInterface&& other) noexcept; // Move constructor
    ~NetworkInterface() noexcept;

    bool operator==(const ip::Ipv4Address& addr) const;
    bool operator!=(const ip::Ipv4Address& addr) const;

    // Represents an entry of a remote interface on the same link as this interface
    // Consists of the iface's virtual IP address and the UDP socket for sending datagrams
    struct NetworkInterfaceEntry {
        ip::Ipv4Address ipAddress_;  // The virtual IP address of the remote interface
        sockaddr_in udpSockAddr_;
        // The following is used when listing neighbors with `lr`
        ip::Ipv4Address udpAddr_;    // The UDP address for link emulation
        in_port_t udpPort_;      // The UDP port for link emulation
    };
    using InterfaceEntries = std::vector<NetworkInterfaceEntry>;

    // Returns an iterator (in neighborInterfaces_) to the next-hop interface on the same link as this interface
    InterfaceEntries::const_iterator findNextHopInterface(const ip::Ipv4Address &nextHop) const;

    // Start receiving incoming datagrams
    void startListening();

    // Receive a single datagram from udp_sock_ and submit it to the thread pool of the network node
    void recvDatagram() const;

    /**
     * Sends the datagram to the next-hop interface in neighborInterfaces_,
     * effectively emulating the link layer with UDP communication.
     * Called in RouterNode::forwardDatagram_().
     */
    void sendDatagram(const ip::Datagram &datagram, const ip::Ipv4Address &nextHop) const;

    void  turnOn() { isUp_ =  true; std::cout << "Interface " << name_ << " is up\n";   };
    void turnOff() { isUp_ = false; std::cout << "Interface " << name_ << " is down\n"; };

    bool  isOn() const { return isUp_; }
    bool isOff() const { return !isUp_; }

private:
    // NetworkInterface objects can be constructed only by NetworkNode
    using NodeDatagramSubmitter = std::function<void(DatagramPtr, const ip::Ipv4Address &)>;
    NetworkInterface(NodeDatagramSubmitter submitter,
                     const std::string &cidr, 
                     const std::vector<std::string> &neighborIpAddrs,
                     const std::vector<in_port_t> &neighborUdpPorts,
                     const std::vector<std::string> &neighborUdpAddrs,
                     in_port_t udpPort,
                     std::string name);

private:
    // Local area network
    ip::Ipv4Address ipAddress_;  // Virtual IP address of this interface
    in_addr_t subnetMask_;   // 32-bit subnet mask of this interface (in host byte order)
    std::size_t subnetMaskLength_;  // Number of bits in subnetMask_ that are 1
    InterfaceEntries neighborInterfaces_;  // Interfaces on the same link as this interface 

    // UDP socket emulating the network interface; the link layer is emulated as UDP communication
    int udp_sock_;

    // Interface thread for receiving datagrams from udp_sock_
    std::thread recvThread_;

    std::string name_;       // Name of the interface
    bool isUp_ = true;       // Whether the interface is up

    // A function member passed from network node (host xor router) this interface is attached to
    // The interface uses this function to submit received datagrams to the network node
    using DatagramSubmitter = std::function<void(DatagramPtr)>;
    DatagramSubmitter datagramSubmitter_;
};

} // namespace tns
