#ifndef USCRIPT_MSGBUF_META_HPP
#define USCRIPT_MSGBUF_META_HPP

#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/hana.hpp>

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

    constexpr explicit operator Field() const
    {
        return {type, name};
    }

    constexpr Field operator()() const
    {
        return *this;
    }
};

namespace rng
{

template<
    uint64_t X = 1,
    uint64_t Y = 1,
    uint64_t Z = 1,
    uint64_t W = 1>
class KissBase
{
public:
    using next = KissBase<6906969069ULL * X + 1234567ULL,
        ((Y ^ (Y << 13)) ^ ((Y ^ (Y << 13)) >> 17)) ^
        (((Y ^ (Y << 13)) ^ ((Y ^ (Y << 13)) >> 17)) << 11),
        Z + ((Z << 26) + W),
        ((Z + ((Z << 26) + W)) >> 6) + (Z + ((Z << 26) + W) < ((Z << 26) + W))>;

    constexpr static uint64_t value = X + Y + Z;
};

constexpr uint64_t hash(uint64_t x)
{
    constexpr std::array<uint64_t, 16> table{
        0x7ad870c830358979ULL, 0x8632586aca9a2710ULL,
        0x2e675fb4576761d0ULL, 0xd9810c6891bd5c04ULL,
        0x8f689158505e9b8bULL, 0xb3ba5c8932b0836dULL,
        0x854fb3003f739fa5ULL, 0x2aca3b2d1a053f9dULL,
        0x8464ea266c2b0836ULL, 0x460abd1952db919fULL,
        0xb99d7ed15d9d8743ULL, 0xf5b0e190606b12f2ULL,
        0xe98f353477ad31a7ULL, 0x5fb354025b277b14ULL,
        0x6debdf121b863991ULL, 0x358804e3f82aa47dULL,
    };
    uint64_t out = x ^ table[x & 0xF];
    out ^= std::rotl(x, 20) ^ table[(x >> 10) & 0xF];
    out ^= std::rotl(x, 10) ^ table[(x >> 30) & 0xF];
    out ^= std::rotr(x, 10) ^ table[(x >> 60) & 0xF];
    return out;
}

constexpr uint64_t time_seed()
{
    return hash(__TIME__[0]) ^ hash(__TIME__[1]) ^
           hash(__TIME__[2]) ^ hash(__TIME__[3]) ^
           hash(__TIME__[4]) ^ hash(__TIME__[5]) ^
           hash(__TIME__[6]) ^ hash(__TIME__[7]);
}

constexpr uint64_t kiss_seed = time_seed();

template<uint64_t Iteration, uint64_t Seed>
class Kiss
{
private:
    friend Kiss<Iteration + 1, Seed>;
    using current = Kiss<Iteration - 1, Seed>::current::next;

public:
    constexpr static uint64_t value = current::value;
};

template<uint64_t Seed>
class Kiss<0ULL, Seed>
{
private:
    friend Kiss<1ULL, Seed>;

    using current = KissBase<
        Seed ^ 1066149217761810ULL,
        Seed ^ 362436362436362436ULL,
        Seed ^ 1234567890987654321ULL,
        Seed ^ 123456123456123456ULL>::next;

public:
    constexpr static uint64_t value = current::value;
};

template<uint64_t Iteration>
constexpr static uint64_t KissValue = Kiss<Iteration, kiss_seed>::value;

} // namespace rng

} // namespace ::umb::meta

#endif // USCRIPT_MSGBUF_META_HPP
