#include "catch_amalgamated.hpp"
#include <ip/util.hpp>

using namespace tns;
using namespace ip;
using namespace tns::ip::util;

TEST_CASE("ip::util::parseCidr") {
    SECTION("Parse 192.168.1.0/24") {
        auto subnet = parseCidr("192.168.1.0/24");
        REQUIRE(subnet.has_value());
        REQUIRE(subnet->address.toStringAddr() == "192.168.1.0");
        REQUIRE(subnet->maskLength == 24);
        REQUIRE(subnet->mask == 0xFFFFFF00);
    }

    SECTION("Invalid mask") {
        REQUIRE( !parseCidr("127.0.0.1/99") );
        REQUIRE( !parseCidr("127.0.0.1/") );
        REQUIRE( !parseCidr("127.0.0.1/33") );
        REQUIRE( !parseCidr("127.0.0.1/-1") );
        REQUIRE( !parseCidr("127.0.0.1/99999999999999999999") );
    }

    SECTION("Invalid address") {
        REQUIRE( !parseCidr("999.999.999.999/24") );
        REQUIRE( !parseCidr("?/24") );
        REQUIRE( !parseCidr("xyz.abc.def.ghi/24") );
    }
}

TEST_CASE("ip::util::makeIpv4Header") {
    Ipv4Address src("192.168.1.1");
    Ipv4Address dest("192.168.1.2");
    uint8_t protocol = IPPROTO_TCP;
    uint16_t payloadLength = 20;

    auto header = makeIpv4Header(src, dest, protocol, payloadLength);

    REQUIRE(header);
    REQUIRE(header->ihl == 5);  // Assuming default for IPv4 header
    REQUIRE(header->version == 4); // Assuming IPv4
    REQUIRE(ntohs(header->tot_len) == 20 + payloadLength); // header length + payload
    REQUIRE(header->protocol == protocol);
    REQUIRE(ntohl(header->saddr) == src.getAddrHost());
    REQUIRE(ntohl(header->daddr) == dest.getAddrHost());
    REQUIRE(header->ttl == INIT_TTL);
}

TEST_CASE("util::ip::ipv4Checksum") {
    SECTION("Example from Wikipedia") {
        uint16_t hdr[] = {0x4500, 0x0073, 0x0000, 0x4000, 0x4011,  0xb861,  0xc0a8, 0x0001, 0xc0a8, 0x00c7};
        auto checksum = ipv4Checksum(hdr, 5);
        REQUIRE(checksum == 0xb861);
    }

    SECTION("All zeros") {
        uint16_t hdr[] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  0xFFFF,  0x0000, 0x0000, 0x0000, 0x0000};
        auto checksum = ipv4Checksum(hdr, 5);
        REQUIRE(checksum == 0xFFFF);
    }

    SECTION("All ones") {
        uint16_t hdr[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  0x0000,  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
        auto checksum = ipv4Checksum(hdr, 5);
        REQUIRE(checksum == 0x0000);
    }
}

TEST_CASE("util::ip::sameSubnet") {
    SECTION("Subnets - /24 mask") {
        Ipv4Address ip1("192.168.1.1");
        Ipv4Address ip2("192.168.1.2");
        Ipv4Address ip3("192.168.2.1");
        in_addr_t mask = 0xFFFFFF00; // /24 subnet

        REQUIRE(sameSubnet(ip1, ip2, mask));
        REQUIRE_FALSE(sameSubnet(ip1, ip3, mask));
    }

    SECTION("Same subnet - /16 mask") {
        Ipv4Address ip1("192.168.1.1");
        Ipv4Address ip2("192.168.200.1");
        in_addr_t mask = 0xFFFF0000;
        REQUIRE(sameSubnet(ip1, ip2, mask));
    }

    SECTION("Different subnet - /16 mask") {
        Ipv4Address ip1("192.168.1.1");
        Ipv4Address ip2("193.168.1.1");
        in_addr_t mask = 0xFFFF0000;
        REQUIRE_FALSE(sameSubnet(ip1, ip2, mask));
    }

    SECTION("Edge case - All zeros mask") {
        Ipv4Address ip1("192.168.1.1");
        Ipv4Address ip2("203.0.113.45");
        in_addr_t mask = 0x00000000;
        REQUIRE(sameSubnet(ip1, ip2, mask));  // All addresses are in the same subnet with a mask of all zeros
    }

}

