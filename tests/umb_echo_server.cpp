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

#include <format>
#include <iostream>
#include <memory>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>

#include "umb/umb.hpp"
#include "TestMessages.umb.hpp"

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

awaitable<void> echo(tcp::socket socket)
{
    try
    {
        std::shared_ptr<umb::Message> msg;
        std::array<umb::byte, 2048> data{};
        std::array<umb::byte, 2048> msg_buf{};

        for (;;)
        {
            std::size_t n = co_await socket.async_read_some(boost::asio::buffer(data),
                                                            use_awaitable);

            // Parse UMB message.
            // Serialize it to bytes, send it back.

            const auto size = static_cast<size_t>(data[0]);
            std::cout << std::format("size: {}\n", size);

            if (n < size)
            {
                std::cout << std::format("got {} bytes, size indicates {}, waiting for more...\n",
                                         n, size);
                throw std::runtime_error("TODO: handle this");
            }

            const auto part = data[1];
            std::cout << std::format("part: {}\n", part);

            const auto type = static_cast<testmessages::umb::MessageType>(
                data[2] | (data[3] << 8));
#if WINDOWS
            const auto mt_str = std::to_string(static_cast<uint16_t>(type));
#else
            const auto mt_str = testmessages::umb::meta::to_string(type);
#endif
            std::cout << std::format("type: {}\n", mt_str);

            const auto payload = std::span<umb::byte>{data.data(), size};

            switch (type)
            {
                case testmessages::umb::MessageType::GetSomeStuff:
                    msg = std::make_shared<testmessages::umb::GetSomeStuff>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::BoolPackingMessage:
                    msg = std::make_shared<testmessages::umb::BoolPackingMessage>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::testmsg:
                    msg = std::make_shared<testmessages::umb::testmsg>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::JustAnotherTestMessage:
                    msg = std::make_shared<testmessages::umb::JustAnotherTestMessage>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::DualStringMessage:
                    msg = std::make_shared<testmessages::umb::DualStringMessage>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::MultiStringMessage:
                    msg = std::make_shared<testmessages::umb::MultiStringMessage>();
                    msg->from_bytes(payload);
                    break;
                case testmessages::umb::MessageType::STATIC_BoolPackingMessage:
                    msg = std::make_shared<testmessages::umb::STATIC_BoolPackingMessage>();
                    msg->from_bytes(payload);
                    break;

                case testmessages::umb::MessageType::None:
                default:
                    throw std::runtime_error(
                        std::format("invalid MessageType: {}",
                                    std::to_string(static_cast<uint16_t>(type))));
            }

            const auto bytes_out = msg->to_bytes();
            const auto size_out = msg->serialized_size();

            co_await async_write(socket, boost::asio::buffer(bytes_out, size_out), use_awaitable);
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
    try
    {
        boost::asio::io_context io_context(1);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           { io_context.stop(); });

        co_spawn(io_context, listener(), detached);

        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cout << std::format("Exception: {}\n", e.what());
    }
}
