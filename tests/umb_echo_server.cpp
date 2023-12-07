/*
 * Copyright (C) 2023  Tuomo Kriikkula
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
 *
 * ****************************************************************************
 *
 * Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

#define WINDOWS 1

// Silence "Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately".
#include <SDKDDKVer.h>

#else

#define WINDOWS 0

#endif

#include <expected>
#include <format>
#include <iostream>
#include <memory>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>

#include "umb/umb.hpp"
#include "TestMessages.umb.hpp"

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::deferred;
using boost::asio::as_tuple;
namespace this_coro = boost::asio::this_coro;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

enum class Error
{
    invalid_size,
    boost_error,
    todo,
};

struct Header
{
    umb::byte size{};
    umb::byte part{};
    testmessages::umb::MessageType type{};
};

void print_header(const Header& header)
{
    std::cout << std::format("size: {}\n", header.size);
    std::cout << std::format("part: {}\n", header.part);

#if WINDOWS
    const auto mt_str = std::to_string(static_cast<uint16_t>(header.type));
#else
    const auto mt_str = testmessages::umb::meta::to_string(header.type);
#endif
    std::cout << std::format("type: {}\n", mt_str);
}

boost::asio::experimental::coro<void() noexcept, std::expected<Header, Error>>
read_header(
    tcp::socket& socket,
    std::array<umb::byte, umb::g_packet_size>& data)
{
    std::cout << std::format("reading {} bytes\n", umb::g_header_size);
    const auto [ec, num_read] = co_await boost::asio::async_read(
        socket,
        boost::asio::buffer(data),
        boost::asio::transfer_exactly(umb::g_header_size),
        as_tuple(deferred));

    if (ec != std::errc())
    {
        co_return std::unexpected(Error::boost_error);
    }

    const auto size = data[0];

    if (size == 0)
    {
        co_return std::unexpected(Error::invalid_size);
    }

    const auto part = data[1];
    const auto type = static_cast<testmessages::umb::MessageType>(
        data[2] | (data[3] << 8));

    const Header hdr = {
        .size = size,
        .part = part,
        .type = type,
    };
    co_return hdr;
}

boost::asio::experimental::coro<void() noexcept, std::expected<void, Error>>
handle_single_part_msg(
    tcp::socket& socket,
    size_t size,
    testmessages::umb::MessageType type,
    std::array<umb::byte, umb::g_packet_size>& data)
{
    // TODO: pass in span into this function, with the header bytes already
    //   assumed to be "skipped" by the caller?
    const auto read_num = size - umb::g_header_size;
    auto d = std::span{data.begin() + umb::g_header_size, data.size() - umb::g_header_size};
    const auto [ec, num_read] = co_await boost::asio::async_read(
        socket,
        boost::asio::buffer(d),
        boost::asio::transfer_exactly(read_num),
        as_tuple(deferred));

    const auto payload = std::span<umb::byte>{data.data(), size};
    std::shared_ptr<umb::Message> msg;

    switch (type)
    {
        case testmessages::umb::MessageType::GetSomeStuff:
            msg = std::make_shared<testmessages::umb::GetSomeStuff>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::GetSomeStuffResp:
            msg = std::make_shared<testmessages::umb::GetSomeStuffResp>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::JustAnotherTestMessage:
            msg = std::make_shared<testmessages::umb::JustAnotherTestMessage>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::testmsg:
            msg = std::make_shared<testmessages::umb::testmsg>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::BoolPackingMessage>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::STATIC_BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::STATIC_BoolPackingMessage>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::MultiStringMessage:
            msg = std::make_shared<testmessages::umb::MultiStringMessage>();
            msg->from_bytes(payload);
            break;
        case testmessages::umb::MessageType::DualStringMessage:
            msg = std::make_shared<testmessages::umb::DualStringMessage>();
            msg->from_bytes(payload);
            break;

        case testmessages::umb::MessageType::None:
        default:
            /*
            throw std::runtime_error(
                std::format("invalid MessageType: {}",
                            std::to_string(static_cast<uint16_t>(type))));
            */
            co_return std::unexpected(Error::todo);
    }

    const auto bytes_out = msg->to_bytes();

    // TODO: check ec.
    std::cout << std::format("sending {} bytes\n", bytes_out.size());
    co_await async_write(
        socket,
        boost::asio::buffer(bytes_out),
        deferred);
}

boost::asio::experimental::coro<void() noexcept, std::expected<void, Error>>
handle_multi_part_msg(
    tcp::socket& socket,
    size_t size,
    testmessages::umb::MessageType type,
    std::array<umb::byte, umb::g_packet_size>& data)
{
    // TODO: use asio::streambuf?

    std::vector<umb::byte> msg_buf;
    umb::byte part = 0;
    umb::byte next_part;

    // TODO: pass in span into this function, with the header bytes already
    //   assumed to be "skipped" by the caller?
    auto read_num = size - umb::g_header_size;
    auto d = std::span{data.begin() + umb::g_header_size, data.size() - umb::g_header_size};
    std::cout << std::format("reading {} bytes\n", read_num);
    const auto [ec, num_read] = co_await boost::asio::async_read(
        socket,
        boost::asio::buffer(d),
        boost::asio::transfer_exactly(read_num),
        as_tuple(deferred));

    msg_buf.reserve(read_num);
    msg_buf.insert(msg_buf.end(), data.cbegin(), data.cend());

    next_part = part + 1;
    while (part != umb::g_part_multi_part_end)
    {
        const auto result = co_await read_header(socket, data);
        if (!result.has_value())
        {
            const auto err = result.error();
            std::cout << std::format("error: {}\n", static_cast<int>(err));
            // TODO
            co_return std::unexpected(err);
        }

        const auto header = *result;
        print_header(header);

        if (header.part != next_part && header.part != umb::g_part_multi_part_end)
        {
            // TODO
            std::cout << std::format("expected part {}, got {}\n", next_part, header.part);
            co_return std::unexpected(Error::todo);
        }

        if (header.type != type)
        {
            // TODO
            std::cout << std::format("expected type {}, got {}\n",
                                     static_cast<uint64_t>(type),
                                     static_cast<uint16_t>(header.type));
            co_return std::unexpected(Error::todo);
        }

        read_num = header.size - umb::g_header_size;
        std::cout << std::format("reading {} bytes\n", read_num);
        const auto [ec, num_read] = co_await boost::asio::async_read(
            socket,
            boost::asio::buffer(data),
            boost::asio::transfer_exactly(read_num),
            as_tuple(deferred));

        msg_buf.reserve(msg_buf.size() + read_num);
        msg_buf.insert(msg_buf.end(), data.cbegin(), data.cend());

        part = header.part;
        next_part = part + 1;
    }

    std::shared_ptr<umb::Message> msg;

    switch (type)
    {
        case testmessages::umb::MessageType::GetSomeStuff:
            msg = std::make_shared<testmessages::umb::GetSomeStuff>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::GetSomeStuffResp:
            msg = std::make_shared<testmessages::umb::GetSomeStuffResp>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::JustAnotherTestMessage:
            msg = std::make_shared<testmessages::umb::JustAnotherTestMessage>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::testmsg:
            msg = std::make_shared<testmessages::umb::testmsg>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::BoolPackingMessage>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::STATIC_BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::STATIC_BoolPackingMessage>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::MultiStringMessage:
            msg = std::make_shared<testmessages::umb::MultiStringMessage>();
            msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::DualStringMessage:
            msg = std::make_shared<testmessages::umb::DualStringMessage>();
            msg->from_bytes(msg_buf);
            break;

        case testmessages::umb::MessageType::None:
        default:
            // TODO
            co_return std::unexpected(Error::todo);
//            throw std::runtime_error(
//                std::format("invalid MessageType: {}",
//                            std::to_string(static_cast<uint16_t>(type))));
    }

    const auto bytes_out = msg->to_bytes();

    const auto num_hdr_bytes = static_cast<size_t>(std::ceil(
        static_cast<float>(bytes_out.size() - umb::g_header_size) /
        static_cast<float>(umb::g_packet_size)) * umb::g_header_size);
    const auto num_parts_out = static_cast<size_t>(std::ceil(
        static_cast<float>(num_hdr_bytes + (bytes_out.size() - umb::g_header_size)) /
        static_cast<float>(umb::g_packet_size)));
    const auto total_bytes_to_send = bytes_out.size() + num_hdr_bytes - umb::g_header_size;

    const auto mt0 = bytes_out[2];
    const auto mt1 = bytes_out[3];
    std::array<umb::byte, umb::g_packet_size> send_buf{};

    // Skip header from bytes_out. It will be written in the loop below.
    unsigned bytes_sent = umb::g_header_size;
    unsigned offset = umb::g_header_size;

    std::cout << std::format(
        "bytes_out: {}, num_hdr_bytes: {}, num_parts_out: {}, total_bytes_to_send: {}\n",
        bytes_out.size(), num_hdr_bytes, num_parts_out, total_bytes_to_send);

    // TODO: rethink this. We want a neat way of doing this... Is this neat?
    for (size_t part_out = 0; part_out < num_parts_out; ++part_out)
    {
        if (part_out == (num_parts_out - 1))
        {
            part_out = umb::g_part_multi_part_end;
        }

        const auto num_to_send = std::min(umb::g_packet_size, total_bytes_to_send - bytes_sent);
        send_buf[0] = num_to_send;
        send_buf[1] = part_out;
        send_buf[2] = mt0;
        send_buf[3] = mt1;

        const std::span<const umb::byte> send_data{bytes_out.cbegin() + offset,
                                                   bytes_out.cend()};
        std::copy_n(send_data.cbegin(), num_to_send - umb::g_header_size, send_buf.begin());

        // TODO: check ec.
        std::cout << std::format("sending {} bytes, offset: {}, part_out: {}\n",
                                 num_to_send, offset, part_out);
        co_await async_write(
            socket,
            boost::asio::buffer(send_buf, num_to_send),
            deferred);

        bytes_sent += num_to_send;
        offset = bytes_sent - umb::g_header_size;
    }
}

// TODO: close connection on bad data, error, etc.?
awaitable<void> echo(tcp::socket socket)
{
    try
    {
        std::cout
            << std::format("connection: {}:{}",
                           socket.remote_endpoint().address().to_string(),
                           socket.remote_endpoint().port())
            << std::endl;

        // TODO: should we read into a larger buffer?
        std::array<umb::byte, umb::g_packet_size> data{};

        for (;;)
        {
            // 1. read 1 byte, it should be the packet size.
            //   - if 255, we can expect to receive more than 255 bytes!
            //     - next byte indicates part, value (0-254), 254 end marker always
            //     - if part is 255, only a single part of size 255 expected
            //   - elif < 255, expect only that amount
            //     - part should always be 255

            auto rh = read_header(socket, data);
            const auto result = co_await rh.async_resume(use_awaitable);

            if (!result.has_value())
            {
                const auto err = result.error();
                // TODO: error messages.
                std::cout << std::format("error: {}\n", static_cast<int>(err));
                continue; // TODO: close conn?
            }

            const auto header = *result;

            // Parse UMB message.
            // Serialize it to bytes, send it back.

            print_header(header);

            // Part == 255       -> handle single-part read
            // Part == 0         -> begin multipart read
            // Part in [1, 253]  -> read multipart bytes
            // Part == 254       -> end multipart read

            if (header.part == ::umb::g_part_single_part)
            {
                auto handle_single = handle_single_part_msg(socket, header.size, header.type, data);
                const auto hs_result = co_await handle_single.async_resume(use_awaitable);
                if (!hs_result.has_value())
                {
                    // TODO
                }
            }
            else if (header.part == 0)
            {
                // TODO: should size always be 255 in this case?
                //   - allow multi-part messages with only a single part,
                //     where the part is smaller than 255?
                //   - PROBABLY SHOULD NOT BE A THING!

                // read parts into buffer until last part
                auto handle_multi = handle_multi_part_msg(socket, header.size, header.type, data);
                const auto hm_result = co_await handle_multi.async_resume(use_awaitable);
                if (!hm_result.has_value())
                {
                    // TODO
                }
            }
            else
            {
                std::cout << std::format("error: unexpected part: {}\n", header.part);
                continue; // Close conn?
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout << std::format("echo Exception: {}\n", e.what());
    }
}

awaitable<void> listener()
{
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), 55555});
    for (;;)
    {
        tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
        co_spawn(executor, echo(std::move(socket)), detached);
    }
}

int main()
{
    std::cout << std::unitbuf;

    try
    {
        boost::asio::io_context io_context(1);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           {
                               std::cout << "exiting" << std::endl;
                               io_context.stop();
                           });

        co_spawn(io_context, listener(), detached);

        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cout << std::format("Exception: {}\n", e.what());
    }
}
