#include <vector>
#include <string>

#include "src/util/lnx_parser/lnxconfig.h"


namespace tns {

namespace util::lnx {
    struct ParsedIfaceData {
        std::string name;
        std::string cidr;
        std::vector<std::string> ip_addrs;
        std::vector<in_port_t> udp_ports;
        std::vector<std::string> udp_addrs;
        uint16_t udp_port;
    };

    struct RoutingData {
        std::string destAddr;
        std::string nextHop;
    };

    struct NetworkNodeData {
        std::vector<ParsedIfaceData> interfaces;
        std::vector<RoutingData> routes;
        std::vector<std::string> ripNeighbors;
    };

    NetworkNodeData parseLnx(const char *filePath);
}

} // namespace tns
