#pragma once

#include <span>
#include <vector>
#include <memory>
#include <functional>

inline auto THROW_NO_IMPL = 
    []() { throw std::runtime_error{"This is a placeholder repo and does not contain implementation."
                                    "Please contact me if you want to take a look at the codebase!"}; };

namespace tns {

class NetworkInterface;

namespace util::threading {
    class PeriodicThread;
} // namespace util::threading

namespace ip {
    class Datagram;
    // enum class Protocol : std::uint8_t;
} // namespace ip

using Payload = std::vector<std::byte>;
using PayloadPtr = std::unique_ptr<Payload>;
using PayloadView = std::span<const std::byte>;
// using PayloadHandler = std::function<void(PayloadPtr)>;

using NetworkInterfaces = std::vector<NetworkInterface>;
using NetworkInterfaceIter = NetworkInterfaces::iterator;

using PeriodicThreadPtr = std::unique_ptr<util::threading::PeriodicThread>;

using DatagramSharedPtr = std::shared_ptr<ip::Datagram>;
using DatagramPtr = std::unique_ptr<ip::Datagram>;
using DatagramHandler = std::function<void(DatagramPtr)>;

} // namespace tns
