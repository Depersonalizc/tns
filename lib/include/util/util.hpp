#pragma once

#include <endian.h>   // __BYTE_ORDER __LITTLE_ENDIAN
#include <algorithm>  // std::reverse()

namespace tns {
namespace util {

// Borrowed from Boost
template <typename T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
inline constexpr T hton(T value) noexcept
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    auto ptr = reinterpret_cast<char *>(&value);
    std::reverse(ptr, ptr + sizeof(T));
#endif
    return value;
}

template <typename T>
inline constexpr T ntoh(T value) noexcept
{
    return hton(value);
}

} // namespace util
} // namespace tns
