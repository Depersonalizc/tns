#pragma once

#include <endian.h>   // __BYTE_ORDER __LITTLE_ENDIAN
#include <algorithm>  // std::reverse()

namespace tns {
namespace util {

// Borrowed from Boost
template <typename T>
inline void hash_combine(std::size_t &seed, const T &v);

template <typename T>
inline constexpr T hton(T value) noexcept;

template <typename T>
inline constexpr T ntoh(T value) noexcept;

} // namespace util
} // namespace tns
