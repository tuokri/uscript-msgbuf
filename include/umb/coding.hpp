/*
 * Copyright (C) 2023-2024  Tuomo Kriikkula
 * This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 *     along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef USCRIPT_MSGBUF_CODING_HPP
#define USCRIPT_MSGBUF_CODING_HPP

#pragma once

// TODO: when encoding dynamic fields, truncate fields longer
//  than maximum size silently? Better than returning an error?

#include <charconv>
#include <concepts>
#include <format>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <vector>


#include "umb/constants.hpp"

static_assert(sizeof(float) * std::numeric_limits<unsigned char>::digits == 32,
              "require 32 bits floats");

namespace umb
{

template<typename T = bool>
concept BoolType = std::is_convertible_v<T, bool>;

// TODO: instead of throwing, use std::expected?
//   - decoding functions should return std::expected<DECODED_TYPE, ERROR_CODE>
//   - how to deal with encoding errors? just return the code without std::expected?

template<typename T = const byte>
inline constexpr bool
_check_bounds_no_throw_impl(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    std::size_t field_size) noexcept
{
    const auto bsize = bytes.size();
    // TODO: should we error here for negative distance?
    const auto dist = std::distance(bytes.cbegin(), i);
    const std::size_t d = static_cast<std::size_t>(dist) + field_size;
    return (d > 0 && (d <= bsize));
}

template<typename T = const byte>
inline constexpr bool
check_bounds_no_throw(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    std::size_t field_size) noexcept
{
    return _check_bounds_no_throw_impl(i, bytes, field_size);
}

template<typename T = const byte>
inline constexpr bool
check_bounds_no_throw(
    const typename std::span<T>::const_iterator&& i,
    const std::span<T> bytes,
    std::size_t field_size) noexcept
{
    return _check_bounds_no_throw_impl(
        std::forward<const typename std::span<T>::const_iterator>(i),
        bytes,
        field_size);
}

template<typename T = const byte>
inline constexpr void
check_bounds(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    std::size_t field_size,
    const std::string& caller = __builtin_FUNCTION())
{
    if (!check_bounds_no_throw(i, bytes, field_size))
    {
        const auto bsize = bytes.size();
        // TODO: should we error here for negative distance?
        const auto dist = std::distance(bytes.cbegin(), i);
        const long long d = dist + static_cast<long long>(field_size);
        const auto msg = std::format("{}: not enough bytes {} {}", caller, d, bsize);
        throw std::out_of_range(msg);
    }
}

template<typename T = const byte>
inline constexpr void
check_bounds(
    const typename std::span<T>::const_iterator&& i,
    const std::span<T> bytes,
    std::size_t field_size,
    const std::string& caller = __builtin_FUNCTION())
{
    check_bounds(
        std::forward<const typename std::span<T>::const_iterator>(i),
        bytes,
        field_size,
        caller);
}

inline constexpr void
check_dynamic_length(std::size_t str_size)
{
    // Max string payload is (g_max_dynamic_size * g_sizeof_uscript_char).
    // Max bytes payload is simply g_max_dynamic_size.
    if (str_size > g_max_dynamic_size)
    {
        throw std::invalid_argument(std::format("dynamic field too large: {}", str_size));
    }
}

inline constexpr void
decode_bool(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    bool& out)
{
    check_bounds(i, bytes, g_sizeof_byte);
    out = *i++;
}

template<BoolType B, std::integral T>
inline constexpr void
_read_bool_single_byte(
    byte b,
    T& index,
    B& out)
{
    out = static_cast<bool>(b & 1 << index++);
}

template<BoolType B, std::integral T>
inline constexpr void
_read_bool_many_bytes(
    byte& b,
    T& index,
    B& out,
    std::span<const byte>::const_iterator& i)
{
    _read_bool_single_byte(b, index, out);
    if ((index % g_bools_in_byte) == 0)
    {
        b = *i++;
        index = 0;
    }
}

template<BoolType... Bools>
inline constexpr void
decode_packed_bools(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    Bools& ... out)
{
    constexpr std::size_t num_bools = sizeof...(out);
    constexpr std::size_t bytes_to_read = (num_bools / g_bools_in_byte) + 1;
    check_bounds(i, bytes, bytes_to_read);
    auto b = *i++;
    std::size_t index = 0;

    // Get bit as bool.
    if constexpr (bytes_to_read > 1)
    {
        ([&]
        {
            _read_bool_many_bytes(b, index, out, i);
        }(), ...);
    }
    else
    {
        ([&]
        {
            _read_bool_single_byte(b, index, out);
        }(), ...);
    }
}

inline constexpr void
decode_uint16(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    uint16_t& out)
{
    check_bounds(i, bytes, g_sizeof_uint16);
    out = (
        static_cast<uint16_t>(*i++)
        | static_cast<uint16_t>(*i++) << 8
    );
}

inline constexpr void
decode_int32(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    int32_t& out)
{
    check_bounds(i, bytes, g_sizeof_int32);
    out = (
        static_cast<int32_t>(*i++)
        | static_cast<int32_t>(*i++) << 8
        | static_cast<int32_t>(*i++) << 16
        | static_cast<int32_t>(*i++) << 24
    );
}

inline constexpr void
decode_byte(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    byte& out)
{
    check_bounds(i, bytes, g_sizeof_byte);
    out = *i++;
}

/**
 * Decode a float from its UMB wire format into a
 * float and its intermediate string representation.
 * \see encode_float.
 * \see encode_float_str.
 *
 * @param i input byte iterator to current position in \bytes.
 * @param bytes input UMB packet bytes being decoded.
 * @param out output float to write the decoded result to.
 * @param serialized_float_cache output string to write the float's
 *  intermediate string representation to.
 */
inline constexpr void
decode_float(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    float& out,
    std::string& serialized_float_cache)
{
    byte size;
    decode_byte(i, bytes, size);

    if (size == 0)
    {
        out = 0;
        return;
    }

    check_bounds(i, bytes, size);

    const std::span<const byte> str_bytes{i, i + size};
    std::advance(i, size);

    std::string float_str;
    float_str.reserve(size);

    for (const auto& sb: str_bytes)
    {
        const char c = static_cast<char>(sb);
        float_str.append(1, c);
    }

    float f;
    const auto [_, ec] = std::from_chars(float_str.data(), float_str.data() + float_str.size(), f);
    if (ec == std::errc())
    {
        out = f;
        serialized_float_cache = std::move(float_str);
    }
    else
    {
        throw std::runtime_error(
            std::format("TODO: decode_float: better handling: {}: '{}'",
                        std::make_error_condition(ec).message(),
                        float_str));
    }
}

/**
 * Decode UMB wire format string of 16-bit characters
 * into a string object. \See encode_string.
 * Overwrites \out contents with the decoded result.
 *
 * @param i input byte iterator to current position in \bytes.
 * @param bytes input UMB packet bytes being decoded.
 * @param out output string to write the decode result to.
 */
inline constexpr void
decode_string(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    std::u16string& out)
{
    byte str_size;
    decode_byte(i, bytes, str_size);

    if (str_size == 0)
    {
        out.clear();
        return;
    }

    const auto payload_size = str_size * g_sizeof_uscript_char;
    check_bounds(i, bytes, payload_size);

    const auto ps = static_cast<std::span<const byte>::difference_type>(payload_size);
    const std::span<const byte> byte_buf{i, i + ps};
    std::advance(i, ps);

    std::vector<char16_t> char_buf;
    char_buf.reserve(str_size);

    char16_t previous = 0x00;
    byte shift = 0;
    for (const auto& bb: byte_buf)
    {
        if (shift == 8)
        {
            char_buf.emplace_back(static_cast<char16_t>(previous | (bb << shift)));
        }
        else
        {
            previous = static_cast<char16_t>(bb);
        }
        // [0, 8, 0, 8, 0, 8, ...]
        shift = (shift + 8) % 16;
    }

    out.clear();
    out.reserve(str_size);
    out.append(char_buf.data(), char_buf.size());
}

/**
 * Decode a dynamic UMB wire format byte sequence
 * (header + payload) into a byte sequence.
 *
 * @param i input byte iterator to current position in \bytes.
 * @param bytes input UMB packet bytes being decoded.
 * @param out output vector to write decoded bytes to.
 */
inline constexpr void
decode_bytes(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    std::vector<byte>& out)
{
    byte size;
    decode_byte(i, bytes, size);

    if (size == 0)
    {
        out.clear();
        return;
    }

    check_bounds(i, bytes, size);
    std::vector<byte> b;
    b.reserve(size);
    while (i != bytes.cend() && b.size() < size)
    {
        b.emplace_back(*i++);
    }
    out = std::move(b);
}

inline constexpr void
encode_bool(bool b, std::span<byte>::iterator& bytes)
{
    *bytes++ = b;
}

template<BoolType B, std::integral T>
inline constexpr void
_write_bool_single_byte(
    std::reference_wrapper<byte> byte_out,
    T& index,
    B in)
{
    const auto set_bit = static_cast<byte>(in) << index++;
    byte_out.get() |= set_bit;
}

// TODO: this has to use different parameter order due to how parameter packs work.
// TODO: normalize signatures across all encoding functions to take the iterator first?
template<BoolType... Bools>
inline constexpr void
encode_packed_bools(std::span<byte>::iterator& bytes, Bools... bools)
{
    auto byte_out = std::ref(*bytes++);
    constexpr std::size_t num_bools = sizeof...(bools);
    constexpr std::size_t bytes_to_write = (num_bools / g_bools_in_byte) + 1;
    std::size_t index = 0;

    if constexpr (bytes_to_write > 1)
    {
        ([&]()
        {
            _write_bool_single_byte(byte_out, index, bools);
            if ((index % g_bools_in_byte) == 0)
            {
                byte_out = std::ref(*bytes++);
                index = 0;
            }
        }(), ...);
    }
    else
    {
        ([&]()
        {
            _write_bool_single_byte(byte_out, index, bools);
        }(), ...);
    }
}

/**
 * Encode a byte into its UMB wire format.
 *
 * @param b input byte to encode.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_byte(byte b, std::span<byte>::iterator& bytes)
{
    *bytes++ = b;
}

/**
 * Encode a 16 bit unsigned integer into its UMB wire format.
 *
 * @param i input integer to encode.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_uint16(uint16_t i, std::span<byte>::iterator& bytes)
{
    *bytes++ = i & 0xff;
    *bytes++ = (i >> 8) & 0xff;
}

/**
 * Encode a 32 bit signed integer into its UMB wire format.
 *
 * @param i input integer to encode.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_int32(int32_t i, std::span<byte>::iterator& bytes)
{
    *bytes++ = i & 0xff;
    *bytes++ = (i >> 8) & 0xff;
    *bytes++ = (i >> 16) & 0xff;
    *bytes++ = (i >> 24) & 0xff;
}

/**
 * Encode a float's *encoded string* representation into input
 * byte iterator in UMB wire format. This function should be
 * used on the encoded string produced by \encode_float.
 *
 * @param float_str encoded string representation of a float.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_float_str(const std::string& float_str, std::span<byte>::iterator& bytes)
{
    const auto size = float_str.size();
    *bytes++ = static_cast<byte>(size);
    for (const auto& c: float_str)
    {
        *bytes++ = static_cast<byte>(c);
    }
}

/**
 * Encode a float into its intermediate encoded string format.
 * The output of this function can be consumed by \encode_float_str
 * to encode the string into its final UMB wire format.
 *
 * @param f input float to encode.
 * @param out output string to write the result to.
 */
inline UMB_CONSTEXPR void
encode_float(float f, std::string& out)
{
    std::string str;
    constexpr auto pre = std::numeric_limits<float>::max_digits10;
    constexpr auto longest_float = std::numeric_limits<float>::digits
                                   - std::numeric_limits<float>::min_exponent;
    // Reserve space for longest possible float string + special chars.
    constexpr auto max_str = longest_float + 8;
    str.resize(max_str);
    constexpr auto fmt = std::chars_format::scientific;
    const auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), f, fmt, pre);
    if (ec == std::errc())
    {
        str.erase(str.find('\0'));
        out = std::move(str);
    }
    else
    {
        throw std::runtime_error(
            std::format("TODO: better handling: {}, {}", f,
                        std::make_error_condition(ec).message()));
    }
}

/**
 * Encode a string of 16-bit characters into its UMB wire format.
 * Effectively a UTF-16 string, but with characters supported in the Unicode
 * range [0, 65535]. Mirrors UE3 internal string representation, so the encoding
 * is more correctly UCS-2 with only the Basic Multilingual Plane (BMP) supported.
 *
 * TODO: endianness issues? Is there a chance the machines using UMB to
 *   communicate use differing endianness for UTF16/UCS-2 strings?
 *   Windows *should* always use UTF16-LE, but is there a chance UE3
 *   clients/servers on other platforms use UTF16-BE?
 *
 * @param str input string to encode.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_string(const std::u16string& str, std::span<byte>::iterator& bytes)
{
    const auto str_size = str.size();
    check_dynamic_length(str_size);
    *bytes++ = static_cast<byte>(str_size);

    if (str_size == 0)
    {
        return;
    }

    for (const char16_t& c: str)
    {
        *bytes++ = static_cast<byte>(c & 0xff);
        *bytes++ = static_cast<byte>((c >> 8) & 0xff);
    }
}

/**
 * Encode a sequence of bytes into its UMB wire format.
 *
 * @param bytes_in input bytes to encode.
 * @param bytes output iterator to write encoded bytes to.
 */
inline constexpr void
encode_bytes(const std::span<const byte> bytes_in, std::span<byte>::iterator& bytes)
{
    const auto size = bytes_in.size();
    check_dynamic_length(size);
    *bytes++ = static_cast<byte>(size);

    if (size == 0)
    {
        return;
    }

    for (const auto& b: bytes_in)
    {
        *bytes++ = b;
    }
}

} // namespace umb

#endif // USCRIPT_MSGBUF_CODING_HPP
