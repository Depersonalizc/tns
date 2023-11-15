#include "ip/datagram.hpp"
#include "ip/util.hpp"
#include "src/util/util.hpp"  // private header

#include <cstring>    // std::memcpy
#include <sstream>    // std::stringstream
// #include <iostream>   // std::cout
#include <algorithm>  // std::any_of
#include <stdexcept>  // std::runtime_error, std::length_error

namespace tns {
namespace ip {

// Used when sending
Datagram::Datagram(const Ipv4Address &srcAddr, 
                   const Ipv4Address &destAddr, 
                   PayloadPtr payload, 
                   ip::Protocol protocol) : payload_(std::move(payload))
{
    ::THROW_NO_IMPL();
}

tl::expected<DatagramPtr, std::string> Datagram::recvDatagram(int sock)
{
    ::THROW_NO_IMPL();
}

} // namespace ip
} // namespace tns
