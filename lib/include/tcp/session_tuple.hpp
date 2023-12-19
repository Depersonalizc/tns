#pragma once

#include "ip/address.hpp"

namespace tns {
namespace tcp {

struct SessionTuple {
    ip::Ipv4Address local;
    ip::Ipv4Address remote;

    bool operator==(const SessionTuple& other) const {
        return local == other.local && remote == other.remote;
    }
};

} // namespace tcp
} // namespace tns


// Simple hash function for a SessionTuple
template <>
struct std::hash<tns::tcp::SessionTuple> {
    std::size_t operator()(const tns::tcp::SessionTuple& tuple) const
    {
        using tns::util::hash_combine;
        std::uint32_t addrL = tuple.local.getAddrNetwork();
        std::uint32_t addrR = tuple.remote.getAddrNetwork();
        std::uint32_t ports = (static_cast<std::uint32_t>(tuple.local.getAddrNetwork()) << 16) | tuple.remote.getAddrNetwork();
        
        std::size_t seed = 0;
        hash_combine(seed, addrL);
        hash_combine(seed, addrR);
        hash_combine(seed, ports);
        return seed;
    }
};
