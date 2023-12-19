#include "src/util/lnx_parser/parse_lnx.hpp"
#include "src/util/lnx_parser/list.h"

#include <arpa/inet.h>
#include <cstring>
#include <unordered_map>
#include <stdexcept>
#include <sstream>

#define LNX_IFNAME_MAX 64


namespace tns {

using namespace std;

// Interface name to its neighbors' info
static unordered_map<string, vector<string>> ifaceNameToIpAddrs;
static unordered_map<string, vector<in_port_t>> ifaceNameToUdpPorts;
static unordered_map<string, vector<string>> ifaceNameToUdpAddrs;

namespace util::lnx {

    static void createNeighbor(lnx_neighbor_t *neighbor);
    static ParsedIfaceData createInterface(lnx_interface_t *interface);
    static RoutingData createStaticRoute(lnx_static_route_t *route);
    static string createRipNeighbor(lnx_rip_neighbor_t *ripNeighbor);

    NetworkNodeData parseLnx(const char *filePath)
    {
        lnxconfig_t *config = lnxconfig_parse(filePath);
        if (config == nullptr)
            throw std::runtime_error("Error parsing lnx config file");
        
        // All the data we need to create a network node
        NetworkNodeData networkNodeData;

        // Populate all the ifaceNameTo____ maps
        lnx_neighbor_t *neighbor;
        list_iterate_begin(&config->neighbors, neighbor, lnx_neighbor_t, link) {
            createNeighbor(neighbor);
        } list_iterate_end();

        // Create interfaces
        lnx_interface_t *interface;
        list_iterate_begin(&config->interfaces, interface, lnx_interface_t, link) {
            auto iface = createInterface(interface);
            networkNodeData.interfaces.emplace_back(iface);
        } list_iterate_end();

        // Create routes
        lnx_static_route_t *route;
        list_iterate_begin(&config->static_routes, route, lnx_static_route_t, link) {
            auto sroute = createStaticRoute(route);
            networkNodeData.routes.emplace_back(sroute);
        } list_iterate_end();

        // Create rip neighbors
        lnx_rip_neighbor_t *ripNeighbor;
        list_iterate_begin(&config->rip_neighbors, ripNeighbor, lnx_rip_neighbor_t, link) {
            auto neighbor = createRipNeighbor(ripNeighbor);
            networkNodeData.ripNeighbors.emplace_back(neighbor);
        } list_iterate_end();

        lnxconfig_destroy(config);
        return networkNodeData;
    }

    // neighbor is the input lnx_neighbor_t struct
    // Push the neighbor's info into the corresponding vectors in ifaceNameTo____.
    static void createNeighbor(lnx_neighbor_t *neighbor) 
    {
        char destAddr[INET_ADDRSTRLEN];
        char udpAddr[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &neighbor->dest_addr, destAddr, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &neighbor->udp_addr, udpAddr, INET_ADDRSTRLEN);

        string ifName = string(neighbor->ifname);  // neighbor->ifname: whose neighbor this is

        // ip addr
        if (ifaceNameToIpAddrs.find(ifName) == ifaceNameToIpAddrs.end()) {
            vector<string> ipAddrs;
            ifaceNameToIpAddrs[ifName] = ipAddrs;
        }
        ifaceNameToIpAddrs[ifName].emplace_back(destAddr);

        // udp addr
        if (ifaceNameToUdpAddrs.find(ifName) == ifaceNameToUdpAddrs.end()) {
            vector<string> udpAddrs;
            ifaceNameToUdpAddrs[ifName] = udpAddrs;
        }
        ifaceNameToUdpAddrs[ifName].emplace_back(udpAddr);

        // udp port
        if (ifaceNameToUdpPorts.find(ifName) == ifaceNameToUdpPorts.end()) {
            vector<in_port_t> udpPorts;
            ifaceNameToUdpPorts[ifName] = udpPorts;
        }
        in_port_t udpPort = neighbor->udp_port;
        ifaceNameToUdpPorts[ifName].emplace_back(udpPort);
    }

    static ParsedIfaceData createInterface(lnx_interface_t *interface) 
    {
        char assignedIp[INET_ADDRSTRLEN];
        char udpAddr[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &interface->assigned_ip, assignedIp, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &interface->udp_addr, udpAddr, INET_ADDRSTRLEN);

        char interfaceName[LNX_IFNAME_MAX];
        strcpy(interfaceName, interface->name);
        int prefixLen = interface->prefix_len;
        uint16_t udpPort = interface->udp_port;

        stringstream cidrStream("");
        cidrStream << assignedIp << "/" << prefixLen;

        ParsedIfaceData data;
        data.name = string(interfaceName);
        data.cidr = cidrStream.str();
        data.ip_addrs = ifaceNameToIpAddrs[interfaceName];
        data.udp_ports = ifaceNameToUdpPorts[interfaceName];
        data.udp_addrs = ifaceNameToUdpAddrs[interfaceName];
        data.udp_port = udpPort;

        return data;
    }

    // Will there always be just one route per lnx file?
    static RoutingData createStaticRoute(lnx_static_route_t *route) 
    {
        RoutingData routingData;
        
        char networkAddr[INET_ADDRSTRLEN];
        char nextHopAddr[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &route->network_addr, networkAddr, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &route->next_hop, nextHopAddr, INET_ADDRSTRLEN);

        stringstream cidrStream("");
        cidrStream << networkAddr << "/" << route->prefix_len;

        routingData.destAddr = cidrStream.str();
        routingData.nextHop = nextHopAddr;

        return routingData;
    }

    static string createRipNeighbor(lnx_rip_neighbor_t *ripNeighbor) 
    {
        char ripNeighborAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ripNeighbor->dest, ripNeighborAddr, INET_ADDRSTRLEN);
        return string(ripNeighborAddr);
    }

}

} // namespace tns
