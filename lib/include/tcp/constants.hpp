#pragma once

#include <limits>
#include <cstdint>

namespace tns {
namespace tcp {

inline constexpr std::size_t SEND_BUFFER_SIZE = std::numeric_limits<std::uint16_t>::max();
inline constexpr std::size_t RECV_BUFFER_SIZE = std::numeric_limits<std::uint16_t>::max();

inline constexpr std::size_t MAX_TCP_PAYLOAD_SIZE = 1360UL;  // 1400 (Datagram::MAX_DATAGRAM_SIZE) - 20 (IP header) - 20 (TCP header)

inline constexpr std::size_t MAX_RETRANSMISSIONS = 5;
inline constexpr auto RETRANSMIT_THREAD_PERIOD = std::chrono::milliseconds{250};

inline constexpr auto SOCKET_REAPER_THREAD_PERIOD = std::chrono::seconds{1};

} // namespace tcp
} // namespace tns
