#ifndef USCRIPT_MSGBUF_CODING_HPP
#define USCRIPT_MSGBUF_CODING_HPP

#pragma once

#include <span>
#include <vector>
#include <string>

namespace umb
{

inline constexpr void check_bounds(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    size_t field_size,
    const std::string& caller = __builtin_FUNCTION())
{
    const auto bsize = bytes.size();
    const auto d = std::distance(bytes.cbegin(), i) + field_size;
    if (d <= 0 || d > (bsize))
    {
        // TODO: use fmtlib...
        const std::string msg =
            caller + ": not enough bytes " + std::to_string(d) + " " +
            std::to_string(bsize);
        throw std::out_of_range(msg);
    }
}

inline constexpr auto decode_int(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    int32_t& out)
{
    check_bounds(i, bytes, g_sizeof_int);
    out = (
        static_cast<int32_t>(*i++)
        | static_cast<int32_t>(*i++) << 8
        | static_cast<int32_t>(*i++) << 16
        | static_cast<int32_t>(*i++) << 24
    );
    return i;
}

inline constexpr auto decode_float(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    float& out)
{
    check_bounds(i, bytes, g_sizeof_float);
    const auto int_part = static_cast<float>(
        static_cast<int32_t>(*i++)
        | static_cast<int32_t>(*i++) << 8
        | static_cast<int32_t>(*i++) << 16
        | static_cast<int32_t>(*i++) << 24
    );
    const auto frac_part = static_cast<float>(
        static_cast<int32_t>(*i++)
        | static_cast<int32_t>(*i++) << 8
        | static_cast<int32_t>(*i++) << 16
        | static_cast<int32_t>(*i++) << 24
    );
    out = int_part + (frac_part / ::umb::g_float_multiplier);
    return i;
}

inline constexpr auto decode_byte(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    byte& out)
{
    check_bounds(i, bytes, g_sizeof_byte);
    out = *i++;
    return i;
}

inline constexpr auto decode_string(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    std::u16string& out)
{
    byte size;
    decode_byte(i, bytes, size);
    size *= g_size_of_uscript_char;
    check_bounds(i, bytes, size);
    std::u16string str;
    str.reserve(size);
    // TODO: is there a better way to do this conversion?
    for (const auto& byte = *i; i < (i + size); ++i)
    {
        const char16_t c = (
            static_cast<uint16_t>(byte)
            | (static_cast<uint16_t>(byte) << 8)
        );
        str.append(&c);
    }
    out = str;
    return i;
}

inline constexpr auto decode_bytes(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    std::vector<byte>& out)
{
    byte size;
    check_bounds(i, bytes, g_sizeof_byte);
    decode_byte(i, bytes, size);
    std::vector<byte> b;
    b.reserve(size);
    for (const auto& byte = *i; i < (i + size); ++i)
    {
        b.emplace_back(byte);
    }
    return i;
}

} // namespace umb

#endif // USCRIPT_MSGBUF_CODING_HPP
