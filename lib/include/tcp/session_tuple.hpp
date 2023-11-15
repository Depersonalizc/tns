#pragma once

#include "ip/address.hpp"
#include "util/defines.hpp"

namespace tns {
namespace tcp {

struct SessionTuple {
    ip::Ipv4Address local;
    ip::Ipv4Address remote;

    bool operator==(const SessionTuple& other) const;
};

} // namespace tcp
} // namespace tns


// Simple hash function for a SessionTuple
template <>
struct std::hash<tns::tcp::SessionTuple> {
    std::size_t operator()(const tns::tcp::SessionTuple& tuple) const
    {
        ::THROW_NO_IMPL();
    }
};
