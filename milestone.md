# IP Milestone

## Design

### Network Interface

`class NetworkInterface` represents an interface of a host or a router.

[TODO]

### Network Node

`class NetworkNode` is an abstract class that represents either a host (`HostNode`) or a router (`RouterNode`). 

- The only public interface of a network node is `NetworkNode::listenToAllInterfaces()`, which performs blocking wait for new datagrams in a loop on *all* network interfaces and processes them with `NetworkNode::handleDatagram`.

- A network node has the following internal member variables:

  - `NetworkNode::interfaces_`: all interfaces of the node as a `map<Ipv4Address, NetworkInterface>` collection

  - `NetworkNode::routingTable_`: the routing table of the the node that maps destination IPs to interfaces. 

    Each entry in the table vector is represented as a

    ```c++
    struct RoutingTableEntry {
        Ipv4Address addr;  // addr & mask defines a subnet
        in_addr_t mask;    // host byte order
        std::optional<Ipv4Address> gateway;  // If gateway is not nullopt, requery with gateway
        std::map<Ipv4Address, NetworkInterface>::const_iterator interfaceIt;  // into interfaces_
    }
    ```

  - `NetworkNode::threadPool_`: a thread pool to handle incoming datagrams from `NetworkNode::listenToAllInterfaces`.

- And the following internal methods:

  - `NetworkNode::queryRoutingTable(const Ipv4Address &destIP)` queries the node's routing table for an entry matching the input `destIP`. If no such entry is found then `routingTable_.end()` is returned.
  - `NetworkNode::forwardDatagram(/* TODO */)` sends the the datagram to the correct interface and out of the node from that interface. It internally calls `NetworkNode::queryRoutingTable` and, once the interface has been found, `NetworkInterface::sendDatagram`.
  - `NetworkNode::handleDatagram(/* TODO */)` is a pure virtual function that `HostNode` and `RouterNode` override with their respective handling routing for an incoming datagram. This is the thread function submitted to worker threads in `NetworkNode::listenToAllInterfaces()`.

### Router Node

`class RouterNode ` is a derived class of `NetworkNode` that represents a router in the network topology.

- `RouterNode` overrides the `handleDatagram` method:

  When a datagram is received on a router, it simply forwards the datagram out by calling `NetworkNode::forwardDatagram`.

  - Q1: *Can we assume this bahavior? Can a router also be the destination for a datagram?*
  - Q2: *if (dest == self) send datagram to OS, otherwise forward?*

### Host Node

`class HostNode ` is a derived class of `NetworkNode` that represents a host in the network topology.

- `HostNode` overrides the `handleDatagram` method:

  When a datagram arrives on a host, the interface checks the destination IP against its own, and sends it to OS if they match.

  - Q3: *Can we assume this behavior? (That a host node should consider itself the endpoint of the network, and that the interface receiving the datagram only needs to check against its **own** IP to decide whether it should be kept or discarded)*



## Answers

- What are the different parts of your IP stack and what data structures do they use? How do these parts interact (API functions, channels, shared data, etc.)?
  - See Design
- What fields in the IP packet are read to determine how to forward a packet?
  - Destination IP
    - For hosts, if the destination address does not match the host address, the datagram should be discarded.
    - For routers, consult the routing table - if no entry matches, discard. Otherwise forward to the next hop. If the "link layer" fails (IP not in `neighborInterfaces_`) the packet gets discarded as well.
  - Should also check TTL and checksum - potential discard.
- What will you do with a packet destined for local delivery (ie, destination IP == your node’s IP)?
  - Extract the payload and "send to OS"?
- What structures will you use to store routing/forwarding information?
  - `NetworkNode::routingTable_` as a `vector<RoutingTableEntry>`.
- What happens when a link is disabled? (ie, how is forwarding affected)? 

## More Questions

- Do we need to handle malformed IP datagrams?
- Can we assume a host only has one interface in the project?
- How do we actually tell a node when to remove/add one of its interface?
- (Q1) Do routers only forward packets or can they do something else with the packets?
- (Q3) When a host receives a packet, can we assume it is the final destination?
- (Q3) If a datagram is received on one interface of a host, do we need to check other interfaces too?
- To confirm, hosts can directly send packets to neighboring hosts and the switch is abstracted away right?