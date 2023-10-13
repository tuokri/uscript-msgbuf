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
    const std::string& caller = __builtin_FUNCTION())
{

}

inline constexpr auto decode_int(
    std::span<const byte>::const_iterator i,
    const std::span<const byte> bytes,
    int32_t& out)
{
    const auto bsize = bytes.size();
    const auto d = std::distance(bytes.cbegin(), i) + g_sizeof_int;
    if (d <= 0 || d > (bsize))
    {
        // TODO: use fmtlib...
        const std::string msg =
            std::string{__FUNCTION__} + ": not enough bytes " + std::to_string(d) + " " +
            std::to_string(bsize);
        throw std::out_of_range(msg);
    }

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
    if ()

        out = *i++;
    return i;
}

template<size_t Size>
inline constexpr std::string decode_string(std::span<const byte, Size> bytes)
{
    return {};
}

} // namespace umb

#endif // USCRIPT_MSGBUF_CODING_HPP
