#pragma once

#include <cstdint>

#include "util/defines.hpp"

namespace tns {
namespace ip {
    enum class Protocol : std::uint8_t {
        TEST = 0,
        TCP  = 6,
        RIP  = 200,
    };

    // Default handlers for protocols
    void testProtocolHandler(DatagramPtr datagram); // PROTO_TEST (0): test packet

} // namespace ip
} // namespace tns
