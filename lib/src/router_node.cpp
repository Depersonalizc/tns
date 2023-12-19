#include "router_node.hpp"

#include "ip/datagram.hpp"
#include "ip/rip_message.hpp"
#include "network_interface.hpp"
#include "util/util.hpp"
#include "util/periodic_thread.hpp"

#include "src/util/lnx_parser/parse_lnx.hpp"
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
    // Parse lnx file and initialize base fields
    const auto nodeData = util::lnx::parseLnx(lnxFile.data());
    NetworkNode::initialize_(nodeData);

    // Record router's ripNeighbors_
    for (const auto &ripNeighbor : nodeData.ripNeighbors)
        ripNeighbors_.emplace_back(ripNeighbor);

    // Register RIP handler and start RIP threads
    initializeRip_();

    std::stringstream ss;
    ss << "/********* RouterNode created with " << interfaces_.size() << " interfaces. *********/\n";
    std::cout << ss.str();
}

RouterNode::RouterNode() = default;
RouterNode::RouterNode(const std::string &lnxFile) : RouterNode(std::string_view(lnxFile)) {}
RouterNode::~RouterNode() { std::cout << "RouterNode::~RouterNode() : DONE!\n"; }


void RouterNode::enableInterface(const std::string &name)
{
    const auto iface = findInterface_(name);
    if (iface == interfaces_.end()) {
        std::stringstream ss;
        ss << "RouterNode::enableInterface(): Interface named " << name << " not found\n";
        std::cerr << ss.str();
    }
    else if (iface->isOff()) {
        /**
         * NOTE: MIND THE ORDER of the following two blocks of code:
         * The interface should be brought up *BEFORE* broadcasting the triggered RIP response
         */
        iface->turnOn();

        auto ripResponse = routingTable_->enableLocalRoute_(iface);
        // auto ripResponse = RipMessage::makeResponse(std::move(ripEntries));
        broadcastRipMessage_(ripResponse);  // Send triggered response due to enabled interface
    }
}

void RouterNode::disableInterface(const std::string &name)
{
    const auto iface = findInterface_(name);
    if (iface == interfaces_.end()) {
        std::stringstream ss;
        ss << "RouterNode::disableInterface(): Interface named " << name << " not found\n";
        std::cerr << ss.str();
    }
    else if (iface->isOn()) {
        /**
         * NOTE: MIND THE ORDER of the following two blocks of code:
         * The interface should be taken down *AFTER* the triggered RIP response has been broadcasted 
         */
        auto ripResponse = routingTable_->disableLocalRoute_(iface);  // Set local route metric to infinity
        // auto ripResponse = RipMessage::makeResponse(std::move(ripEntries));
        std::cout << "Sending triggered RIP response due to DISABLED interface\n";
        broadcastRipMessage_(ripResponse);  // Send triggered response due to disabled interface

        iface->turnOff();
    }
}

/**
 * Called when a datagram arrives for this router node.
 * Assume the IP header is valid.
 * Forward the datagram to the next hop.
 */
void RouterNode::datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &/*infaceAddr*/) const
{
    // std::cout << "RouterNode::datagramHandler_(): Datagram arrived at router\n";

    // Check if the destination IP address matches interface's
    // if (inface == datagram->getDstAddr()) {
    if (isMyIpAddress_(datagram->getDstAddr())) {    // Local delivery: let OS consume the datagram
        invokeProtocolHandler_(std::move(datagram));
    } else {                                          // Forward the datagram to the next hop
        datagram->updateChecksum();
        forwardDatagram_(*datagram);
    }
}

// Query the routing table to find an entry with a matching subnet.
// Then send out the datagram through the interface specified by that entry.
void RouterNode::forwardDatagram_(const Datagram &datagram) const
{
    // Query the routing table to find the interface to send the datagram to.
    const Ipv4Address destIP = datagram.getDstAddr();
    const auto nextHop = queryRoutingTable_(destIP);
    if (!nextHop) {
        std::cerr << "RouterNode::forwardDatagram_(): " << nextHop.error() << "\n";
        return;
    }

    // Send the datagram out through that interface.
    const auto &[interface, nextHopAddr] = nextHop.value();
    interface.sendDatagram(datagram, nextHopAddr);
}

void RouterNode::initializeRip_()
{
    using util::threading::PeriodicThread;

    // Register RIP callback
    protocolHandlers_[ip::Protocol::RIP] = 
        [this](DatagramPtr datagram) {ripProtocolHandler_(std::move(datagram));};

    // Spin up the RIP thread to periodically send RIP packets
    ripThread_ = std::make_unique<PeriodicThread>(RIP_INTERVAL, [this]() {
        auto ripResponse = routingTable_->generateRipEntries_();
        if (!ripResponse.getEntries().empty()) {
            // auto ripResponse = RipMessage::makeResponse(std::move(ripEntries));
            broadcastRipMessage_(ripResponse);  // Broadcast rip response to rip neighbors
        }
    });

    // Spin up the RIP cleaner thread to periodically check and remove expired RIP routes
    ripCleanerThread_ = std::make_unique<PeriodicThread>(RIP_INTERVAL, [this]() {
        auto ripResponse = routingTable_->removeStaleRipEntries_(RIP_EXPIRATION_TIME);
        if (!ripResponse.getEntries().empty()) {
            // auto ripResponse = RipMessage::makeResponse(std::move(expiredRipEntries));
            std::cout << "Sending triggered RIP response due to EXPIRED entries\n";
            broadcastRipMessage_(ripResponse);  // Send triggered response due to expired routes
        }
    });

    // Broadcast RIP request to neighbors
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        broadcastRipMessage_( RipMessage::makeRequest() );
    }).detach();
}

void RouterNode::ripProtocolHandler_(DatagramPtr datagram)
{
    // std::cout << "RouterNode::ripProtocolHandler_(): Received RIP packet!\n";

    // Incoming RIP packet
    const RipMessage ripMessage{ datagram->getPayloadView() };
    const auto &srcAddr = datagram->getSrcAddr();

    switch (ripMessage.getCommand()) {
    case RipMessage::Command::REQUEST : {
        const auto ripResponse = routingTable_->generateRipEntries_();
        if (!ripResponse.getEntries().empty()) {
            // Send rip response to the neighbor that sent us the request
            // auto ripResponse = RipMessage::makeResponse(std::move(ripEntries));
            sendRipMessage_(ripResponse, srcAddr);
        }
        break;
    }
    case RipMessage::Command::RESPONSE : {
        // Learn new routes, and send triggered response to neighbors if necessary
        const auto ripResponse = 
            routingTable_->handleRipEntries_(ripMessage.getEntries(), srcAddr);
        if (!ripResponse.getEntries().empty()) {
            // Send triggered response of updatedEntries to ripNeighbors_
            std::cout << "Sending triggered RIP response due to UPDATED entries\n";
            broadcastRipMessage_(ripResponse);
        }
        break;
    }
    default:
        std::stringstream ss;
        ss << "RouterNode::ripProtocolHandler_(): Unknown RIP command: " 
           << static_cast<std::uint16_t>(ripMessage.getCommand()) << "\n";
        std::cerr << ss.str();
        break;
    }
}

void RouterNode::broadcastRipMessage_(const RipMessage &ripMessage) const
{
    // MAYBE: Use worker threads with synchronization on ripMessage
    for (const auto &neighbor : ripNeighbors_) {
        // std::cout << "RouterNode::RouterNode(): Sending RIP packets to neighbor " << neighbor << "\n";
        sendRipMessage_(ripMessage, neighbor);
    }
}

void RouterNode::sendRipMessage_(const RipMessage &ripMessage, const Ipv4Address &destIP) const
{
    PayloadPtr payload = std::make_unique<Payload>(ripMessage.payloadSize());

    // Construct payload
    auto it = payload->begin();

    const auto command = util::hton(static_cast<std::uint16_t>(ripMessage.getCommand()));
    it = util::insertData(it, command);

    const auto numEntries = util::hton(ripMessage.getNumEntries());
    it = util::insertData(it, numEntries);

    auto entryIt = ripMessage.getEntries().cbegin();
    auto learnedFromIt = ripMessage.getLearnedFrom().cbegin();
    for (; entryIt != ripMessage.getEntries().end(); entryIt++, learnedFromIt++) {
        auto cost = *learnedFromIt && destIP == (*learnedFromIt).value()
                  ? RipMessage::INFINITY  // Poisoned reverse
                  : entryIt->cost;
        RipMessage::Entry entryNetwork = {
            .cost    = util::hton(cost),
            .address = util::hton(entryIt->address),
            .mask    = util::hton(entryIt->mask)
        };
        it = util::insertData(it, entryNetwork);
    }

    try {
        sendIp_(destIP, std::move(payload), ip::Protocol::RIP);
    } catch (const std::exception &e) {
        std::cerr << "RouterNode::sendRipMessage_(): " << e.what() << "\n";
    }
}

} // namespace tns
