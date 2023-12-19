#include "catch_amalgamated.hpp"
#include <ip/datagram.hpp>
#include <ip/util.hpp>
#include <util/defines.hpp>

using namespace tns;
using namespace tns::ip;
using namespace std;

TEST_CASE("Datagram - Constructors and basic properties") {
    Ipv4Address src("192.168.1.1");
    Ipv4Address dest("192.168.1.2");
    uint8_t protocol = IPPROTO_TCP;
    
    auto il = {byte{0x01}, byte{0x02}, byte{0x03}, byte{0x04}};
    Datagram datagram(src, dest, make_unique<Payload>(il), Protocol(protocol));

    SECTION("Constructor with srcAddr, destAddr, protocol, and payload") {

        REQUIRE(datagram.getDstAddr() == dest);  // Assuming getDestAddr() works correctly.
        REQUIRE(datagram.getSrcAddr() == src);
        REQUIRE(datagram.getProtocol() == Protocol(protocol));
        REQUIRE(datagram.getTotalLength() == 24);  // 20-byte header + 4-byte payload
    }

    // Note: For the Datagram(int sock) constructor, we might want to setup a mock socket or use integration tests
}