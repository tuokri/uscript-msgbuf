#ifndef USCRIPT_MSGBUF_META_HPP
#define USCRIPT_MSGBUF_META_HPP

#pragma once

namespace umb::meta
{

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
        static_assert("invalid type string");
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
            static_assert("");
    }
}

struct Field
{
    const FieldType type;
    const char* const name;
};

}

#endif // USCRIPT_MSGBUF_META_HPP
