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
    {
        std::stringstream ss;
        ss << "\tNetworkInterface::NetworkInterface(): Creating interface " << name_
        << ", subnet: " << cidr 
        << ", udp port: " << udpPort 
        << "\n";
        std::cout << ss.str();
    }

    if (neighborIpAddrs.size() != neighborUdpPorts.size() || 
        neighborIpAddrs.size() != neighborUdpAddrs.size())
        throw std::invalid_argument("NetworkInterface::NetworkInterface(): neighborIpAddrs, "
                                    "neighborUdpPorts, and neighborUdpAddrs must have the same size");

    // Create a sockaddr_in struct for use in socket functions
    sockaddr_in addr = {.sin_family = AF_INET};

    // Use cidr to initialize ipAddress_ and subnetMask_
    auto subnet = ip::util::parseCidr(cidr);  // host byte order
    if (!subnet)
        throw std::runtime_error("NetworkInterface::NetworkInterface(): " + subnet.error());
    std::tie(ipAddress_, subnetMask_, subnetMaskLength_) = subnet.value();

    // Use neighborIpAddrs and neighborUdpPorts to initialize neighborInterfaces_
    for (std::size_t i = 0; i < neighborIpAddrs.size(); i++) {
        // Fill in address
        if (inet_aton(neighborUdpAddrs[i].c_str(), &addr.sin_addr) == 0)
            throw std::system_error(errno, std::generic_category(), 
                "NetworkInterface::NetworkInterface(): inet_aton()");
        
        // ... and port
        addr.sin_port = util::hton(neighborUdpPorts[i]);

        neighborInterfaces_.emplace_back(
            neighborIpAddrs[i], 
            addr,  // used by sendto() calls
            neighborUdpAddrs[i], 
            neighborUdpPorts[i]
        );
    }

    // Sort neighborInterfaces_ by ipAddress_ to make findNextHopInterface() faster
    std::sort(neighborInterfaces_.begin(), neighborInterfaces_.end(), 
        [](const NetworkInterfaceEntry &entry1, const NetworkInterfaceEntry &entry2) {
            return entry1.ipAddress_ < entry2.ipAddress_;
        }
    );

    // Create udp_sock_ for receiving datagrams
    if ((udp_sock_ = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        throw std::system_error(errno, std::generic_category(), 
            "NetworkInterface::NetworkInterface(): socket()");

    // Bind the socket to localhost:udpPort
    addr.sin_port = util::hton(udpPort);
    if (bind(udp_sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
        throw std::system_error(errno, std::generic_category(), 
            "NetworkInterface::NetworkInterface(): bind()");

    // Set up datagram submitter to submit datagrams to the network node
    datagramSubmitter_ = [addr = this->ipAddress_, s = std::move(submitter)](DatagramPtr d) {
        s(std::move(d), addr); 
    };
}

NetworkInterface::~NetworkInterface() noexcept
{
    // Close read socket
    if (udp_sock_ != -1) {
        std::cout << "\tNetworkInterface::~NetworkInterface(): Shutting down read socket " << udp_sock_ << "\n";
        if (shutdown(udp_sock_, SHUT_RDWR) == -1)
            std::cerr << "\tNetworkInterface::~NetworkInterface(): shutdown() failed: " << strerror(errno) << "\n";
        std::cout << "\tNetworkInterface::~NetworkInterface(): Read socket successfully shut down\n";
    }

    // Join recvThread_
    if (recvThread_.joinable()) {
        std::cout << "\tNetworkInterface::~NetworkInterface(): Joining recvThread_ ...\n";
        try {
            recvThread_.join();
            std::cout << "\tNetworkInterface::~NetworkInterface(): DONE\n";
        } catch (const std::system_error &e) {
            std::cerr << "\tNetworkInterface::~NetworkInterface(): recvThread_.join() failed: " << e.what() << "\n";
        }
    }
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
    // other.name_ = "[MOVED]";
}

bool NetworkInterface::operator==(const ip::Ipv4Address& addr) const { return ipAddress_ == addr; }
bool NetworkInterface::operator!=(const ip::Ipv4Address& addr) const { return ipAddress_ != addr; }

void NetworkInterface::startListening()
{
    recvThread_ = std::thread([this]{
        try {
            while (true)
                recvDatagram();
        } catch (const std::runtime_error &e) {
            std::cerr << "\tNetworkInterface::startListening(): " << e.what() << "\n";
        }
    });
}

// Receive a single datagram from udp_sock_ and submit it to the network node
void NetworkInterface::recvDatagram() const
{
    // Receive a datagram from udp_sock_
    auto datagram = ip::Datagram::recvDatagram(udp_sock_);

    if (!datagram) {
        std::cerr << "\tNetworkInterface::recvDatagram(): Datagram::recvDatagram() failed: " 
                  << datagram.error() << "\n";
    } else if (isOn()) {
        datagramSubmitter_(std::move(datagram.value())); // Submit the datagram to the network node
    }
}

/**
 * This method is called when a datagram is presented to this interface from 
 * the router node, i.e., the router will call interface.sendDatagram(datagram).
 * Send the datagram to the correct next-hop interface in neighborInterfaces_.
 */
void NetworkInterface::sendDatagram(const ip::Datagram &datagram, const ip::Ipv4Address &nextHopAddr) const
{
    if (isOff()) return;

    // Find the next-hop interface on the same link
    auto nextHopInterface = findNextHopInterface(nextHopAddr);
    if (nextHopInterface == neighborInterfaces_.cend()) {
        std::stringstream ss;
        ss << "\tNetworkInterface::sendDatagram(): No next-hop interface " 
           << nextHopAddr.toStringAddr() << " found in neighbors\n";
        std::cerr << ss.str();
    } else {
        // Emulate the link layer with UDP communication
        // Send the IP header and payload in a single sendmsg() call
        iovec iov[2] = {
            {.iov_base = (char *)(&datagram.ipHeader_), .iov_len = sizeof(datagram.ipHeader_)}, // Header
            {.iov_base = (char *)(datagram.payload_->data()), .iov_len = datagram.payload_->size()}  // Payload
        };
        msghdr msg = {
            .msg_name = (char *)(&nextHopInterface->udpSockAddr_), 
            .msg_namelen = sizeof(nextHopInterface->udpSockAddr_),
            .msg_iov = iov, .msg_iovlen = 2
        };
        if (sendmsg(udp_sock_, &msg, 0) == -1)
            std::cerr << "\tNetworkInterface::sendDatagram(): sendmsg() failed: " << strerror(errno) << "\n";
    }
}

/* Returns the next-hop interface (in neighborInterfaces_) reachable from this interface */
NetworkInterface::InterfaceEntries::const_iterator
NetworkInterface::findNextHopInterface(const ip::Ipv4Address &nextHopAddr) const
{
    const auto [first, last] = std::equal_range(neighborInterfaces_.cbegin(), neighborInterfaces_.cend(),
        NetworkInterfaceEntry{.ipAddress_ = nextHopAddr},
        [](const NetworkInterfaceEntry &entry1, const NetworkInterfaceEntry &entry2) -> bool {
            return entry1.ipAddress_ < entry2.ipAddress_;
        }
    );

    return first == last ? neighborInterfaces_.cend() : first;
}

} // namespace tns
