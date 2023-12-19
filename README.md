[IP](https://brown-csci1680.github.io/iptcp-docs/) | [TCP](https://brown-csci1680.github.io/iptcp-docs/tcp-handout/)

## How to Build the Project
In the root directory of the project,
```
mkdir build && cd build
cmake ..
make -j4
```
This will build a `libiptcp.so` shared library, and two executables `vhost` and `vrouter` that link to the library.

# TCP

## System Design
The socket APIs are defined in `tcp_stack.hpp`, including vConnect, vListen, vSend, vRecv and vClose. `tcp_stack.hpp` also has a tcpProtocolHandler which finds the corresponding socket and calls the event handler of the listen or normal socket.  

`buffers.hpp` contains definitions for `SendBuffer` and `RecvBuffer`, both of which inherit from `RingBuffer`. `SendBuffer` 's `write` method writes data to the send buffer and moves the `nbw_` pointer to one past the last byte written. `write` also blocks until there is free space in the buffer. `SendBuffer`'s `onAck` method is called with an ACK is received. It updates the `una_` pointer if the received ACK number is within the expected range. After updating, it notifies all waiting writter threads that there is free space and removes packets that are entirely acknowledged from the retransmission queue. `sendReadyData` moves the `nxt_` pointer to the 'sent but un-acked' and returns the sequence number on the packet and the length of payload, which is then fed into the `sendPacket` method of the `Socket` class and then the constructor of the `Payload` class. In the `RecvBuffer`, the `readAtMostBytes` takes in a buffer and a number `n` and reads up to `n` bytes into the provided buffer. It blocks if the receive buffer is empty and advances the `nbr_` pointer by `n`. The `onRecv` method handles an incoming segment and also early arrivals. Per early arrivals, it views all segments as intervals, inserts the new interval and merges all overlapping intervals. Otherwise, it merges and removes all early arrival segments and reduces the window. The two methods related to merging intervals are defined in `intervals.hpp`. 

The `Packet` class has a header field and a payload field. It also has constructors that construct different types of packets. 

In the `RetransmissionQueue` class, there are is an enqueue method and a getExpiredEntry method which returns all entries that have reached the maximum number of retransmission. The method which removes all packets that are entirely acknowledged from the queue (used in the `Buffer` class) is also defined here. This class also contains methods to calculate RTT. 

The `SessionTuple` class constructor creates an instance with local and remote addresses and ports and defines a hash function for the class. 

The `SocketError` class defines 8 types of socket errors: "connection closing", "timeout", "connection reset", "connection does not exist", "connection already exists", "insufficient resources", "op not allowed", "not yet implemented" and "unknown". 

The `Socket` class is inherited by two classes, `ListenSocket` and `NormalSocket`. In `ListenSocket` class, it defines `vAccept`, which accepts connections by dequeueing an established socket from the accept queue. `vAccept` blocks until a new connection is available or an error occurs. `AcceptQueue` is a struct defined within `ListenSocket` and it defines a series of methods: `onClose` aborts all sockets in the accept queue; `pushAndNotify` pushes a new socket to the accept queue and notifies the `vAccept` about the readiness of the new socket; `waitAndPop` does the job of `vAccept`. `PendingSocks` is another struct defined in `ListenSocket` and it is basically a list of pending connections (SYN-RECEIVED sockets) keyed by the session tuple, with add and remove functions. `NormalSocket` defines `vSend`, `vReceive`, `vClose` and `vAbort` functions. These functions either do the writing, reading, closing or aborting or throw errors depending on the state stored in the socket. Besides, the retransmit thread is a periodic thread which sends retransmit packets every 100 milliseconds. 

The `states` namespace defines the structs for all the states. 

The `TcpStack` class defines a series of event handler functions which handles receiving a packet of certain type when the host is in certain state. Events are transitions between state and the transitions are defined in this class, including the three-way handshake and the four-way handshake. 

## Known Bugs
1. Sending large files (>100 MB) with packet losses is unstable and connection may be lost prematurely.

2. Simultaneous open and close of connections are not handled.

3. The retransmission queue is not flushed before sending FIN.

# IP

## System Design
We implemented an abstract class `NetworkNode` which is inherited by both `HostNode` and `RouterNode` to represent hosts and routers respectively. The class *NetworkInterface* represents the interfaces of the nodes and the `RoutingTable` class represents the routing table on each node.

### RoutingTable, NetworkInterface
#### Data Structures
In the `RoutingTable` class, we have an `Entry` struct consisting of all the info about an entry: entry type (`RIP`, `LOCAL`, or `STATIC`), addr and mask that defines the subnet, next hop address (named `gateway`), cost (named `metric`), and a last-refresh time. The routing table is basically a vector of `Entry` protected by a mutex. When the network node constructs its routing table, it also passes in a reference to itself so that the routing table can access the interfaces.

In the `NetworkInterface` class, we have a `NetworkInterfaceEntry` struct consisting of the virtual IP address of the interface, a UDP socket that emulates sending an IP packet to a remote interface on the same link, the address and port of the UDP connection. In this class, there is also an vector of neighbor interfaces (addresses). When the network node constructs interfaces, it passes in a callback function (datagram submitter) to allow the interface to submit received datagram to its node's thread pool.

### NetworkNode, HostNode, RouterNode
#### Data Structures
In the `NetworkNode` class, we have a vector of `NetworkInterface`, a routing table, an unordered_map from interface names to interface objects, a map from an interface's virtual IP address to interface objects and a thread pool that handles received datagrams. Each `NetworkNode` object also has a map from IP protocols to its datagram handler. IP protocol can be 0 or 200. Hosts only have a datagram handler for the test protocol while routers have both test protocol handler and RIP protocol handler.

In the `RouterNode` class, we have a vector of Ipv4Addresses representing the router's RIP neighbors.

#### Public API
vhost/vrouter first calls the `HostNode`/`RouterNode` constructor to declare a `HostNode`/`RouterNode` object with the .lnx file provided at the command line and registers a datagram handler for the test protocol. The `RouterNode` constructor also registers the datagram handler for the RIP protocol. It then takes in commands from the user. When the user types a `send` command, it parses the destination IP and message body from user input and then calls `NetworkNode::sendIpTest` to send the test packet. When the user types an `up` command, it calls `NetworkNode::enableInterface` on the node to look up the mapping with interface name as key and turn on the interface. When the user types a `down` command, it calls `NetworkNode::disableInterface` on the node to turn off the interface. The `NetworkNode` class also provides functions to print out all the interfaces, neighbors and routes to be called when user enters a `li`, `ln` or `lr` command.

#### Threading
For each `NetworkNode` object, we have a thread pool to handle incoming datagrams.

For RIP, we have a thread for sending periodic updates every 5 seconds to a router's neighbors, a cleaner thread for cleaning up RIP routes if they are not refreshed for 12 seconds and sending a triggered update to notify neighbors about the deleted entry. The receiving thread for each interface is listening for incoming packets and using the appropriate handler to handle it. In the handler for RIP packets, it responds to RIP requests with the whole routing table or updates the routing table with other routers' RIP responses and broadcasts triggered updates.

#### Processing IP Packets
When nodes receive a packet, they
1. Recompute the checksum and check whether the checksum is correct. If not correct, the packet is dropped.
2. Check if the TTL equals 0 and if so, drop the packet.
3. Check whether the IP header option is zero and drop the packet if not zero.
4. Check if IP header length is greater than the total length and drop the packet if greater.
When the routers receive a packet, they check whether the destination IP on the packet is the same as the router's IP. If so, they invoke the protocol handler to handle test and RIP packets differently. Otherwise, they update the checksum, query the routing table to find the next hop and forward the packet to the next hop.

When the hosts receive a packet, they check whether the destination IP on the packet is the same as the host's IP. If so, they also invoke the protocol handler to handle test packets. Hosts drop the packet and report an error at the command line when they don't have a handler for the protocol on the received packet.

### Other Design Decisions
When a RIP entry's cost becomes infinite or times out, the cleaner thread will get rid of the entry from the routing table. This is designed to not let entries of unreachable nodes waste the resources in the routing table.
