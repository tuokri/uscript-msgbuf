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

#ifndef USCRIPT_MSGBUF_MESSAGE_HPP
#define USCRIPT_MSGBUF_MESSAGE_HPP

#pragma once

#include <span>
#include <string>
#include <vector>

#include "umb/coding.hpp"

namespace umb
{

// TODO: constexpr virtual?

class Message
{
public:
    Message() = default;

    Message(const Message&) = delete;

    Message& operator=(const Message&) = delete;

    Message(Message&&) noexcept = delete;

    Message& operator=(Message&&) noexcept = delete;

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

    /**
     * Return human-readable string representation of this
     * message. This is not portable.
     * TODO: string encoding?
     *
     * @return message as a human-readable string.
     */
    [[nodiscard]] virtual std::wstring to_string() const = 0;

    // TODO: reconsider this API. Don't use uint16_t directly?
    [[nodiscard]] constexpr virtual uint16_t type() const noexcept = 0;

protected:
    [[nodiscard]] virtual bool is_equal(const Message& msg) const = 0;

    friend inline bool operator==(const Message&, const Message&);
};

inline bool operator==(const Message& lhs, const Message& rhs)
{
    return typeid(lhs) == typeid(rhs) && lhs.is_equal(rhs);
}

}

#endif // USCRIPT_MSGBUF_MESSAGE_HPP
