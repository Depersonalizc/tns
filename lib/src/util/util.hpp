#pragma once

#include <climits>     // CHAR_BIT
#include <unistd.h>    // ssize_t
#include <functional>  // std::hash

#include "util/defines.hpp"

namespace tns {

namespace util {
    template <typename T>
    inline constexpr std::size_t bit_size() noexcept { return sizeof(T) * CHAR_BIT; }

    // inline char *toCharPtr(auto ptr) noexcept { return const_cast<char *>(reinterpret_cast<const char *>(ptr)); }

    inline Payload::iterator insertData(Payload::iterator it, const auto &data)
    {
        auto dataPtr = reinterpret_cast<const char *>(&data);
        for (std::size_t i = 0; i < sizeof(data); ++i)
            *it++ = static_cast<std::byte>(*dataPtr++);
        return it;
    }

    namespace io {
        ssize_t recvAll(int sockfd, void *buf, std::size_t len, int flags = 0);
    } // namespace io

} // namespace util

} // namespace tns
