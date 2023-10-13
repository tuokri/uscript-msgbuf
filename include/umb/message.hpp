#ifndef USCRIPT_MSGBUF_MESSAGE_HPP
#define USCRIPT_MSGBUF_MESSAGE_HPP

#pragma once

#include <cstdint>
#include <span>

#include "umb/constants.hpp"
#include "umb/coding.hpp"

namespace umb
{

class Message
{
public:
    virtual ~Message() = default;

    // TODO: can we write to a span? Problem with checking
    //   if span has enough space to write to?
    virtual std::vector<byte> to_bytes() = 0;

    virtual bool from_bytes(std::span<const byte> bytes) = 0;
};

// decode_int
// encode_int
// decode_byte
// encode_byte

class SomeMessage: public Message
{
public:
    ~SomeMessage() override = default;

    std::vector<byte> to_bytes() override
    {
        return {};
    }

    bool from_bytes(const std::span<const byte> bytes) override
    {
        // Check that we have enough bytes to
        // - read all static values
        // - read all dynamic field

        auto i = bytes.cbegin();

        int32_t temp;
        i = decode_int(i, bytes, temp);
        std::cout << std::to_string(temp) << "\n";

        // check bytes.size();
        // m_field = decode_int(bytes[some_index]);

//        const auto size = *i++;
//        if ((size + std::abs(std::distance(bytes.begin(), i))) > bytes.size())
//        {
//
//        }

        return false;
    }
};

}

#endif // USCRIPT_MSGBUF_MESSAGE_HPP
