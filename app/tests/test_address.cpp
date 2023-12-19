#include "catch_amalgamated.hpp"
#include <ip/address.hpp>

using namespace tns;
using namespace tns::ip;

inline constexpr auto TEST_ADDR_STR = "192.168.1.1";
inline constexpr auto TEST_ADDR_NUM_HOST = 0xC0A80101UL;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline constexpr auto TEST_ADDR_NUM_NET = 0x0101A8C0UL;
#else
inline constexpr auto TEST_ADDR_NUM_NET = 0xC0A80101UL;
#endif


TEST_CASE("IPv4 Address - Default Constructor") {
    Ipv4Address ip;
    REQUIRE(ip.toStringAddr() == "0.0.0.0");
    REQUIRE(ip.getAddrHost() == 0);
}

TEST_CASE("IPv4 Address - Constructor with in_addr_t") {
    Ipv4Address ip(TEST_ADDR_NUM_NET); // 192.168.1.1
    REQUIRE(ip.toStringAddr() == TEST_ADDR_STR);
}

TEST_CASE("IPv4 Address - Constructor with sockaddr_in") {
    sockaddr_in addr = {};
    addr.sin_addr.s_addr = TEST_ADDR_NUM_NET;
    Ipv4Address ip(addr);
    REQUIRE(ip.toStringAddr() == TEST_ADDR_STR);
}

TEST_CASE("IPv4 Address - Constructor with C-string") {
    Ipv4Address ip(TEST_ADDR_STR);
    REQUIRE(ip.toStringAddr() == TEST_ADDR_STR);
}

TEST_CASE("IPv4 Address - Constructor with std::string") {
    Ipv4Address ip(std::string{TEST_ADDR_STR});
    REQUIRE(ip.toStringAddr() == TEST_ADDR_STR);
}

TEST_CASE("IPv4 Address - Equality and Inequality Operators") {
    Ipv4Address ip1("192.168.1.1");
    Ipv4Address ip2("192.168.1.1");
    Ipv4Address ip3("192.168.1.2");
    
    REQUIRE(ip1 == ip2);
    REQUIRE(ip1 != ip3);
}

TEST_CASE("IPv4 Address - Less Than Operator") {
    Ipv4Address ip1("192.168.1.1");
    Ipv4Address ip2("192.168.1.2");
    
    REQUIRE(ip1 < ip2);
    REQUIRE_FALSE(ip2 < ip1);
}

TEST_CASE("IPv4 Address - getAddrHost and getAddrNetwork") {
    Ipv4Address ip(TEST_ADDR_STR);
    REQUIRE(ip.getAddrHost() == TEST_ADDR_NUM_HOST);
    REQUIRE(ip.getAddrNetwork() == TEST_ADDR_NUM_NET);
}
