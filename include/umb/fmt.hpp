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

#ifndef USCRIPT_MSGBUF_FMT_HPP
#define USCRIPT_MSGBUF_FMT_HPP

#pragma once

#include <charconv>
#include <format>
#include <limits>
#include <string>
#include <vector>

#include "umb/constants.hpp"

namespace umb::fmt
{

template<typename T>
inline std::wstring to_wstring(const T& t)
{
    return std::to_wstring(t);
}

template<>
inline std::wstring to_wstring(const std::vector<::umb::byte>& t)
{
    std::wstring str;
    str.reserve(2 + (t.size() * 2));
    str += L"[";

    for (const auto& elem: t)
    {
        str += std::format(L"{:x},", elem);
    }

    str += L"]";
    return str;
}

template<>
inline std::wstring to_wstring(const std::u16string& t)
{
    return {t.cbegin(), t.cend()};
}

template<>
inline std::wstring to_wstring(const float& t)
{
    std::string str;

    constexpr auto pre = std::numeric_limits<float>::max_digits10;
    // Reserve space for longest possible scientific float string + special chars.
    constexpr auto max_str = pre + 8;
    str.resize(max_str);

    auto fmt = std::chars_format::scientific;
    const auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), t, fmt, pre);
    if (ec == std::errc())
    {
        return std::wstring{str.cbegin(), str.cend()};
    }

    return L"";
}

}

#endif // USCRIPT_MSGBUF_FMT_HPP
