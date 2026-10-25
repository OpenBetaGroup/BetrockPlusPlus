/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

// Endianness conversion where it's needed, as network packets, NBT data and Java itself are all big endian
// Also endian.h was already taken by something in glibc, so... bweh whatever :p

#include <bit>
#include <cstdint>
#include <type_traits>

#ifdef _MSC_VER
#include <cstdlib>
#define __builtin_bswap16(x) _byteswap_ushort(x)
#define __builtin_bswap32(x) _byteswap_ulong(x)
#define __builtin_bswap64(x) _byteswap_uint64(x)
#include <__msvc_int128.hpp>
using __int128_t = std::_Signed128;
using __uint128_t = std::_Unsigned128;
#endif

template <typename T>
inline T ByteswapAny(T _value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "byteswap_any: only trivially copyable types allowed");

    if constexpr (sizeof(T) == 1) {
        return _value;
    } else if constexpr (sizeof(T) == 2) {
        auto tmp = std::bit_cast<uint16_t>(_value);
        return std::bit_cast<T>(__builtin_bswap16(tmp));
    } else if constexpr (sizeof(T) == 4) {
        auto tmp = std::bit_cast<uint32_t>(_value);
        return std::bit_cast<T>(__builtin_bswap32(tmp));
    } else if constexpr (sizeof(T) == 8) {
        auto tmp = std::bit_cast<uint64_t>(_value);
        return std::bit_cast<T>(__builtin_bswap64(tmp));
    } else {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
                      sizeof(T) == 4 || sizeof(T) == 8,
                      "byteswap_any: unsupported type size");
    }
}

template <typename T>
constexpr T ToBigEndian(T _value) {
    if constexpr (std::endian::native == std::endian::big)
        return _value;
    else
        return ByteswapAny(_value);
}

template <typename T>
constexpr T FromBigEndian(T _value) {
    return ToBigEndian(_value);
}
