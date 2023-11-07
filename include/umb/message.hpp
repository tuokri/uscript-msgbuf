#ifndef USCRIPT_MSGBUF_MESSAGE_HPP
#define USCRIPT_MSGBUF_MESSAGE_HPP

#pragma once

#include <span>
#include <vector>

#include "umb/coding.hpp"

namespace umb
{

// TODO: constexpr virtual?

class Message
{
public:
    virtual ~Message() = default;

    /**
     * Serialize message to UMB wire format byte vector. The vector
     * size will match Message::serialized_size().
     *
     * @return Serialized message bytes.
     */
    [[nodiscard]] virtual std::vector<byte> to_bytes() const = 0;

    /**
     * Serialize message to UMB wire format. \bytes should be
     * pre-allocated to a size returned by Message::serialized_size().
     *
     * @param bytes The span to write the wire format bytes to.
     * @return true if \bytes contains valid UMB wire format
     *         bytes of this message.
     */
    [[nodiscard]] virtual bool to_bytes(std::span<byte> bytes) const = 0;

    /**
     * Initialize message from \bytes span containing valid
     * UMB wire format bytes for this message.
     *
     * @param bytes UMB wire format bytes input span.
     * @return true message was successfully initialized
     *         from \bytes.
     */
    virtual bool from_bytes(std::span<const byte> bytes) = 0;

    /**
     * Return calculated UMB wire format size of this message
     * in bytes. The size may change if message fields are
     * changed.
     *
     * @return serialized UMB wire format size of this
     *         message in bytes.
     */
    [[nodiscard]] virtual size_t serialized_size() const = 0;
};

}

#endif // USCRIPT_MSGBUF_MESSAGE_HPP
