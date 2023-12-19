#pragma once

#include <span>
#include <vector>
#include <memory>
#include <functional>

namespace tns {

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };

class NetworkInterface;

namespace util::threading {
    class PeriodicThread;
} // namespace util::threading

namespace ip {
    class Datagram;
} // namespace ip

using Payload = std::vector<std::byte>;
using PayloadPtr = std::unique_ptr<Payload>;
using PayloadView = std::span<const std::byte>;
// using PayloadHandler = std::function<void(PayloadPtr)>;

using NetworkInterfaces = std::vector<NetworkInterface>;
using NetworkInterfaceIter = NetworkInterfaces::iterator;

using PeriodicThreadPtr = std::unique_ptr<util::threading::PeriodicThread>;
// using PeriodicThreadPtr = std::unique_ptr<util::threading::PeriodicThread, util::threading::PeriodicThreadDeleter>;

using DatagramSharedPtr = std::shared_ptr<ip::Datagram>;
using DatagramPtr = std::unique_ptr<ip::Datagram>;
using DatagramHandler = std::function<void(DatagramPtr)>;

} // namespace tns
