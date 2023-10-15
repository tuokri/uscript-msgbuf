#ifndef USCRIPT_MSGBUF_MESSAGE_HPP
#define USCRIPT_MSGBUF_MESSAGE_HPP

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "umb/coding.hpp"

namespace umb
{

class Message
{
public:
    virtual ~Message() = default;

    // TODO: can we write to a span? Problem with checking
    //   if span has enough space to write to?
    [[nodiscard]] virtual std::vector<byte> to_bytes() const = 0;

    virtual bool to_bytes(std::span<byte> bytes) const = 0;

    virtual bool from_bytes(std::span<const byte> bytes) = 0;

    [[nodiscard]] virtual size_t serialized_size() const = 0;
};

}

#endif // USCRIPT_MSGBUF_MESSAGE_HPP
