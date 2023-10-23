#ifndef USCRIPT_MSGBUF_CODING_HPP
#define USCRIPT_MSGBUF_CODING_HPP

#pragma once

#include <charconv>
#include <concepts>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>

#include "umb/constants.hpp"

static_assert(sizeof(float) * std::numeric_limits<unsigned char>::digits == 32,
              "require 32 bits floats");

namespace umb
{

template<typename T = bool>
concept BoolType = std::is_convertible_v<T, bool>;

// TODO: instead of throwing, use std::expected?

template<typename T = const byte>
inline constexpr bool _check_bounds_no_throw_impl(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    size_t field_size) noexcept
{
    const auto bsize = bytes.size();
    const auto d = std::distance(bytes.cbegin(), i) + field_size;
    return (d > 0 && (d <= bsize));
}

template<typename T = const byte>
inline constexpr bool check_bounds_no_throw(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    size_t field_size) noexcept
{
    return _check_bounds_no_throw_impl(i, bytes, field_size);
}

template<typename T = const byte>
inline constexpr bool check_bounds_no_throw(
    const typename std::span<T>::const_iterator&& i,
    const std::span<T> bytes,
    size_t field_size) noexcept
{
    return _check_bounds_no_throw_impl(
        std::forward<const typename std::span<T>::const_iterator>(i),
        bytes,
        field_size);
}

template<typename T = const byte>
inline constexpr void check_bounds(
    const typename std::span<T>::const_iterator& i,
    const std::span<T> bytes,
    size_t field_size,
    const std::string& caller = __builtin_FUNCTION())
{
    if (!check_bounds_no_throw(i, bytes, field_size))
    {
        const auto bsize = bytes.size();
        const auto d = std::distance(bytes.cbegin(), i) + field_size;
        const auto msg = std::format("{}: not enough bytes {} {}", caller, d, bsize);
        throw std::out_of_range(msg);
    }
}

template<typename T = const byte>
inline constexpr void check_bounds(
    const typename std::span<T>::const_iterator&& i,
    const std::span<T> bytes,
    size_t field_size,
    const std::string& caller = __builtin_FUNCTION())
{
    check_bounds(
        std::forward<const typename std::span<T>::const_iterator>(i),
        bytes,
        field_size,
        caller);
}

inline constexpr void check_dynamic_length(size_t str_size)
{
    // Max string payload is (g_max_dynamic_size * g_sizeof_uscript_char).
    // Max bytes payload is simply g_max_dynamic_size.
    if (str_size > g_max_dynamic_size)
    {
        throw std::invalid_argument(std::format("dynamic field too large: {}", str_size));
    }
}

inline constexpr void decode_bool(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    bool& out)
{
    check_bounds(i, bytes, g_sizeof_byte);
    out = *i++;
}

template<BoolType B, std::integral T>
inline constexpr void _read_bool_single_byte(
    byte b,
    T& index,
    B& out)
{
    out = static_cast<bool>(b & 1 << index++);
}

template<BoolType B, std::integral T>
inline constexpr void _read_bool_many_bytes(
    byte& b,
    T& index,
    B& out,
    std::span<const byte>::const_iterator& i)
{
    _read_bool_single_byte(b, index, out);
    if ((index % g_bools_in_byte) == 0)
    {
        b = *i++;
    }
}

template<BoolType... Bools>
inline constexpr void decode_packed_bools(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    Bools& ... out)
{
    constexpr size_t num_bools = sizeof...(out);
    constexpr size_t bytes_to_read = (num_bools / g_bools_in_byte) + 1;
    check_bounds(i, bytes, bytes_to_read);
    auto b = *i++;
    auto index = 0;

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

inline constexpr void decode_uint16(
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

inline constexpr void decode_int32(
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

inline constexpr void decode_byte(
    std::span<const byte>::const_iterator& i,
    const std::span<const byte> bytes,
    byte& out)
{
    check_bounds(i, bytes, g_sizeof_byte);
    out = *i++;
}

inline constexpr void decode_float(
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
    if (std::from_chars(float_str.data(), float_str.data() + float_str.size(), f).ec == std::errc())
    {
        out = f;
        serialized_float_cache = std::move(float_str);
    }
    else
    {
        throw std::runtime_error("TODO: better handling");
    }
}

inline constexpr void decode_string(
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

    char16_t previous = 0;
    auto shift = 8;
    for (const auto& byte: byte_buf)
    {
        if (shift == 0)
        {
            char_buf.emplace_back((previous << shift) | byte);
        }
        else
        {
            previous = static_cast<char16_t>(byte);
        }
        shift = (shift + 8) % 16;
    }

    out.clear();
    out.reserve(str_size);
    out.append(char_buf.data(), char_buf.size());
}

inline constexpr void decode_bytes(
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
    for (const auto& byte = *i; i != bytes.cend() && b.size() < size; ++i)
    {
        b.emplace_back(byte);
    }
    out = std::move(b);
}

inline constexpr void encode_bool(bool b, std::span<byte>::iterator& bytes)
{
    *bytes++ = b;
}

template<BoolType B, std::integral T>
inline constexpr void _write_bool_single_byte(
    byte& byte_out,
    T& index,
    B in)
{
    byte_out |= static_cast<byte>(in) << index++;
}

template<BoolType B, std::integral T>
inline constexpr void _write_bool_many_bytes(
    byte& byte_out,
    T& index,
    B in,
    std::span<byte>::iterator& bytes)
{
    _write_bool_single_byte(byte_out, index, in);
    if ((index % g_bools_in_byte) == 0)
    {
        byte_out = *bytes++;
    }
}

// TODO: this has to use different parameter order due to how parameter packs work.
// TODO: normalize signatures across all encoding functions to take the iterator first?
template<BoolType... Bools>
inline constexpr void encode_packed_bools(std::span<byte>::iterator& bytes, Bools... bools)
{
    byte& byte_out = *bytes++;
    constexpr size_t num_bools = sizeof...(bools);
    constexpr size_t bytes_to_write = (num_bools / g_bools_in_byte) + 1;
    auto index = 0;

    if constexpr (bytes_to_write > 1)
    {
        ([&]()
        {
            _write_bool_many_bytes(byte_out, index, bools, bytes);
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

inline constexpr void encode_byte(byte b, std::span<byte>::iterator& bytes)
{
    *bytes++ = b;
}

inline constexpr void encode_uint16(uint16_t i, std::span<byte>::iterator& bytes)
{
    *bytes++ = i & 0xff;
    *bytes++ = (i >> 8) & 0xff;
}

inline constexpr void encode_int32(int32_t i, std::span<byte>::iterator& bytes)
{
    *bytes++ = i & 0xff;
    *bytes++ = (i >> 8) & 0xff;
    *bytes++ = (i >> 16) & 0xff;
    *bytes++ = (i >> 24) & 0xff;
}

inline constexpr void
encode_float_str(const std::string& float_str, std::span<byte>::iterator& bytes)
{
    const auto size = float_str.size();
    *bytes++ = std::clamp<byte>(size, 0, g_max_dynamic_size);
    for (const auto& c: float_str)
    {
        *bytes++ = static_cast<byte>(c);
    }
}

inline constexpr void
encode_float(float f, std::string& out)
{
    std::string str;
    str.resize(std::numeric_limits<float>::max_digits10 + 8);
    auto fmt = f < 0 ? std::chars_format::fixed : std::chars_format::scientific;
    if (auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), f, fmt);
        ec == std::errc())
    {
        out = std::move(str);
    }
    else
    {
        throw std::runtime_error("TODO: better handling");
    }
}

inline constexpr void encode_string(const std::u16string& str, std::span<byte>::iterator& bytes)
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
