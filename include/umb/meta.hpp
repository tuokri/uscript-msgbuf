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

#ifndef USCRIPT_MSGBUF_META_HPP
#define USCRIPT_MSGBUF_META_HPP

#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace umb::meta
{

constexpr auto empty_string = "";

template<auto>
struct always_false: std::false_type
{
};

enum class FieldType
{
    None,
    Boolean,
    Byte,
    Integer,
    Float,
    String,
    Bytes,
};

constexpr FieldType from_type_string(const std::string& str)
{
    if (str == "bool")
    {
        return FieldType::Boolean;
    }
    else if (str == "byte")
    {
        return FieldType::Byte;
    }
    else if (str == "int")
    {
        return FieldType::Integer;
    }
    else if (str == "float")
    {
        return FieldType::Float;
    }
    else if (str == "string")
    {
        return FieldType::String;
    }
    else if (str == "bytes")
    {
        return FieldType::Bytes;
    }
    else
    {
        throw std::invalid_argument(std::format("invalid type string: {}", str));
    }
}

constexpr std::string to_string(FieldType ft)
{
    switch (ft)
    {
        case FieldType::Boolean:
            return "Boolean";
        case FieldType::Byte:
            return "Byte";
        case FieldType::Integer:
            return "Integer";
        case FieldType::Float:
            return "Float";
        case FieldType::String:
            return "String";
        case FieldType::Bytes:
            return "Bytes";
        default:
            throw std::invalid_argument(std::format("invalid FieldType: {}", static_cast<int>(ft)));
    }
}

template<FieldType FT, const auto& Name>
struct Field
{
    static constexpr auto type = FT;
    static constexpr auto name = Name;

// TODO: check if this is actually needed for anything.
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wclass-conversion"
//     constexpr explicit operator Field() const
//     {
//         return {type, name};
//     }
// #pragma GCC diagnostic pop

    constexpr Field operator()() const
    {
        return *this;
    }
};

} // namespace ::umb::meta

#endif // USCRIPT_MSGBUF_META_HPP
