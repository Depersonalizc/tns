#include "src/util/lnx_parser/parse_lnx.hpp"
#include "src/util/lnx_parser/list.h"
#include "util/defines.hpp"

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
        ::THROW_NO_IMPL();
    }

    // neighbor is the input lnx_neighbor_t struct
    // Push the neighbor's info into the corresponding vectors in ifaceNameTo____.
    static void createNeighbor(lnx_neighbor_t *neighbor) 
    {
        ::THROW_NO_IMPL();
    }

    static ParsedIfaceData createInterface(lnx_interface_t *interface) 
    {
        ::THROW_NO_IMPL();
    }

    // Will there always be just one route per lnx file?
    static RoutingData createStaticRoute(lnx_static_route_t *route) 
    {
        ::THROW_NO_IMPL();
    }

    static string createRipNeighbor(lnx_rip_neighbor_t *ripNeighbor) 
    {
        ::THROW_NO_IMPL();
    }

}

} // namespace tns
