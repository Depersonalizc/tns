#pragma once

#include <climits>     // CHAR_BIT
#include <unistd.h>    // ssize_t
#include <functional>  // std::hash

#include "util/defines.hpp"

namespace tns {

namespace util {
    template <typename T>
    inline constexpr std::size_t bit_size() noexcept;

    inline Payload::iterator insertData(Payload::iterator it, const auto &data);

    namespace io {
        ssize_t recvAll(int sockfd, void *buf, std::size_t len, int flags = 0);
    } // namespace io

} // namespace util

} // namespace tns
