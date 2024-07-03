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
 *
 * ****************************************************************************
 *
 * Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "umb/umb.hpp"

#if UMB_WINDOWS

// Silence "Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately".
#include <SDKDDKVer.h>

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

#include <unicode/unistr.h>
#include <unicode/ustring.h>

#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "TestMessages.umb.hpp"

namespace
{

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

std::shared_ptr<spdlog::async_logger> g_logger;

std::string bytes_to_string(const std::span<const ::umb::byte> bytes, size_t num_to_take)
{
    if (num_to_take > bytes.size())
    {
        throw std::runtime_error(std::format(
            "num_to_take larger than array ({} > {})", num_to_take, bytes.size()));
    }

    std::stringstream ss;
    ss << "[";
    for (auto i = 0; i < num_to_take; ++i)
    {
        const auto byte = bytes[i];
        // ss << std::format("{:x},", +byte);
        ss << std::format("{},", +byte);
    }
    ss << "]";
    return ss.str();
}

enum class Error
{
    invalid_size,
    boost_error,
    todo,
};

// TODO: should we generate something like this for all generated message
//   "packages"? E.g. testmessages::umb::MessageHeader?
struct Header
{
    umb::byte size{};
    umb::byte part{};
    testmessages::umb::MessageType type{};
};

void print_header(const Header& header)
{
    g_logger->info("size: {}", header.size);
    g_logger->info("part: {}", header.part);
    const auto mt_str = testmessages::umb::meta::to_string(header.type);
    g_logger->info("type: {}", mt_str);
}

// TODO: unify error codes!
//  https://www.boost.org/doc/libs/1_76_0/libs/system/doc/html/system.html
boost::asio::experimental::coro<void() noexcept, std::expected<Header, Error>>
read_header(
    tcp::socket& socket,
    std::array<umb::byte, umb::g_packet_size>& data)
{
    g_logger->info("reading {} bytes", umb::g_header_size);
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
    g_logger->info("sending {} bytes", bytes_out.size());
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
    g_logger->info("reading {} bytes", read_num);
    const auto [read_ec, num_read_actual] = co_await boost::asio::async_read(
        socket,
        boost::asio::buffer(d),
        boost::asio::transfer_exactly(read_num),
        as_tuple(deferred));

    if (read_ec != std::errc())
    {
        g_logger->error("async_read failed: {}, num_read_actual: {}",
                        read_ec.message(), read_num);
    }

    msg_buf.reserve(read_num);
    msg_buf.insert(
        msg_buf.end(),
        data.cbegin(),
        // Header bytes are also included in the first part.
        data.cbegin() + static_cast<std::ptrdiff_t>(read_num) + umb::g_header_size);

    next_part = part + 1;
    while (part != umb::g_part_multi_part_end)
    {
        const auto result = co_await read_header(socket, data);
        if (!result.has_value())
        {
            const auto err = result.error();
            g_logger->error("error: {}", static_cast<int>(err));
            // TODO
            co_return std::unexpected(err);
        }

        const auto header = *result;
        print_header(header);

        if (header.part != next_part && header.part != umb::g_part_multi_part_end)
        {
            // TODO
            g_logger->error("expected part {}, got {}", next_part, header.part);
            co_return std::unexpected(Error::todo);
        }

        if (header.type != type)
        {
            // TODO
            g_logger->error("expected type {}, got {}",
                            static_cast<uint64_t>(type),
                            static_cast<uint16_t>(header.type));
            co_return std::unexpected(Error::todo);
        }

        read_num = header.size - umb::g_header_size;
        g_logger->info("reading {} bytes", read_num);
        const auto [ec, num_read] = co_await boost::asio::async_read(
            socket,
            boost::asio::buffer(data),
            boost::asio::transfer_exactly(read_num),
            as_tuple(deferred));

        if (ec != std::errc())
        {
            g_logger->error("async_read failed: {}, num_read: {}",
                            ec.message(), num_read);
        }

        msg_buf.reserve(msg_buf.size() + read_num);
        msg_buf.insert(
            msg_buf.end(),
            data.cbegin(),
            data.cbegin() + static_cast<std::ptrdiff_t>(read_num));

        part = header.part;
        next_part = part + 1;
    }

    g_logger->info("received msg_buf: {}", bytes_to_string(msg_buf, msg_buf.size()));

    std::shared_ptr<umb::Message> msg;
    bool ok = false;

    switch (type)
    {
        case testmessages::umb::MessageType::GetSomeStuff:
            msg = std::make_shared<testmessages::umb::GetSomeStuff>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::GetSomeStuffResp:
            msg = std::make_shared<testmessages::umb::GetSomeStuffResp>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::JustAnotherTestMessage:
            msg = std::make_shared<testmessages::umb::JustAnotherTestMessage>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::testmsg:
            msg = std::make_shared<testmessages::umb::testmsg>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::BoolPackingMessage>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::STATIC_BoolPackingMessage:
            msg = std::make_shared<testmessages::umb::STATIC_BoolPackingMessage>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::MultiStringMessage:
            msg = std::make_shared<testmessages::umb::MultiStringMessage>();
            ok = msg->from_bytes(msg_buf);
            break;
        case testmessages::umb::MessageType::DualStringMessage:
            msg = std::make_shared<testmessages::umb::DualStringMessage>();
            ok = msg->from_bytes(msg_buf);
            break;

        case testmessages::umb::MessageType::None:
        default:
            // TODO
            co_return std::unexpected(Error::todo);
//            throw std::runtime_error(
//                std::format("invalid MessageType: {}",
//                            std::to_string(static_cast<uint16_t>(type))));
    }

    if (!ok)
    {
        g_logger->error("umb_echo_server ERROR: msg->from_bytes failed for MessageType {}",
                        static_cast<int>(type));
    }

    // TODO: what the fuck is going on here?
    // std::wcout << std::format(L"*** received message: {} ***\n\n\n", msg->to_string()) << std::endl;
    // std::wcout << std::endl;
    // std::cout << std::endl;

    const auto log_msg = std::format(L"*** received message: {} ***\n\n\n", msg->to_string());
#if UMB_WINDOWS
    const auto log_msg_icu = icu::UnicodeString(
        log_msg.c_str(),
        static_cast<int32_t>(log_msg.size()));
    std::string log_msg_str;
    log_msg_icu.toUTF8String(log_msg_str);
#else
    // TODO: this might not work properly. Have to actually test the server on Linux!
    UErrorCode u_err = U_ZERO_ERROR;
    const UChar* size_needed_result = u_strFromWCS(
        nullptr,
        0,
        nullptr,
        log_msg.c_str(),
        static_cast<int32_t>(log_msg.size()),
        &u_err);
    if (U_FAILURE(u_err))
    {
        g_logger->error("ICU error: {}", u_errorName(u_err));
        co_return std::unexpected(Error::todo);
    }
    int32_t len;
    const auto size_needed = *size_needed_result;
    std::vector<UChar> buffer(size_needed);
    u_strFromWCS(
        buffer.data(),
        size_needed,
        &len,
        log_msg.c_str(),
        static_cast<int32_t>(log_msg.size()),
        &u_err);
    const auto log_msg_icu = icu::UnicodeString(
        buffer.data(),
        static_cast<int32_t>(buffer.size()));
    std::string log_msg_str;
    log_msg_icu.toUTF8String(log_msg_str);
#endif
    g_logger->info(log_msg_str);

    const auto bytes_out = msg->to_bytes();
    g_logger->info("bytes_out: {}", bytes_to_string(bytes_out, bytes_out.size()));
    g_logger->info("bytes_out size: {}", bytes_out.size());
    // Only count the number of payload bytes left here.
    auto payload_bytes_left = bytes_out.size() - umb::g_header_size;
    auto it_bytes_out = bytes_out.cbegin();

    // Cache message type values, will be reused for all headers.
    const auto mt0 = bytes_out[2];
    const auto mt1 = bytes_out[3];

    std::array<umb::byte, umb::g_packet_size> send_buf{};
    umb::byte send_size = 0;
    umb::byte send_part = 0;
    unsigned num_from_buf = 0;

    // TODO: REALLY CONSIDER ADDING HEADERS TO MULTIPART MESSAGE ENCODING
    //       ALREADY IN THE ENCODING PHASE. THIS IS A LOT OF ADDED COMPLEXITY
    //       TO PERFORM THE PACKAGE SPLITTING HERE.
    // Send full parts while we have enough bytes left for them.
    while (payload_bytes_left > (umb::g_packet_size - umb::g_header_size))
    {
        send_size = static_cast<umb::byte>(umb::g_packet_size);
        send_buf[0] = send_size;
        send_buf[1] = send_part++;
        send_buf[2] = mt0;
        send_buf[3] = mt1;

        num_from_buf = send_size - umb::g_header_size;
        std::copy_n(
            it_bytes_out + umb::g_header_size,
            num_from_buf,
            send_buf.begin() + umb::g_header_size);
        it_bytes_out += static_cast<umb::byte>(send_size - umb::g_header_size);

        g_logger->info("sending {} bytes, send_part: {}, payload_bytes_left: {}, num_from_buf: {}",
                       send_size, send_part, payload_bytes_left, num_from_buf);
        const auto [sent_ec, num_sent] = co_await async_write(
            socket,
            boost::asio::buffer(send_buf, send_size),
            as_tuple(deferred));

        if (sent_ec != std::errc())
        {
            g_logger->error("async_write failed: {}, num_sent: {}",
                            sent_ec.message(), num_sent);
        }

        g_logger->info("send_buf: {}", bytes_to_string(send_buf, send_size));

        payload_bytes_left -= num_from_buf;
    }

    // Send the final non-full part.
    // TODO: what is the 2nd check here?
    if ((payload_bytes_left > 0) &&
        (payload_bytes_left <= (umb::g_packet_size - umb::g_header_size)))
    {
        send_size = static_cast<umb::byte>(payload_bytes_left) + umb::g_header_size;
        send_part = umb::g_part_multi_part_end;
        send_buf[0] = send_size;
        send_buf[1] = send_part;
        send_buf[2] = mt0;
        send_buf[3] = mt1;

        num_from_buf = send_size - umb::g_header_size;
        std::copy_n(
            it_bytes_out + umb::g_header_size,
            num_from_buf,
            send_buf.begin() + umb::g_header_size);

        // TODO: don't need to do this for last part?
        //       Keep here to detect access beyond end (in debug builds?)?
        it_bytes_out += send_size;

        g_logger->info("sending {} bytes, send_part: {}, payload_bytes_left: {}, num_from_buf: {}",
                       send_size, send_part, payload_bytes_left, num_from_buf);
        const auto [sent_ec, num_sent] = co_await async_write(
            socket,
            boost::asio::buffer(send_buf, send_size),
            deferred);

        g_logger->info("send_buf: {}", bytes_to_string(send_buf, send_size));

        if (sent_ec != std::errc())
        {
            g_logger->error("async_write failed: {}, num_sent: {}",
                            sent_ec.message(), num_sent);
        }
    }
    else
    {
        g_logger->error("ERROR: invalid number of bytes left for last part: {}!",
                        payload_bytes_left);
    }

    // TODO: check bytes left is 0 here.
}

// TODO: close connection on bad data, error, etc.?
awaitable<void> echo(tcp::socket socket)
{
    try
    {
        g_logger->info("connection: {}:{}",
                       socket.remote_endpoint().address().to_string(),
                       socket.remote_endpoint().port());

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
                g_logger->error("error: {}, closing connection\n",
                                static_cast<int>(err));
                break;
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
                g_logger->error("error: unexpected part: {}", header.part);
                continue; // Close conn?
            }
        }
    }
    catch (const std::exception& e)
    {
        g_logger->error("echo error: {}", e.what());
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

} // namespace

int main()
{
    try
    {
        spdlog::init_thread_pool(8192, 1);
        constexpr auto max_log_size = 1024 * 1024 * 10;
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "umb_echo_server.log", max_log_size, 3);
        const auto sinks = std::vector<spdlog::sink_ptr>{stdout_sink, rotating_sink};
        g_logger = std::make_shared<spdlog::async_logger>(
            "umb_echo_server", sinks.begin(), sinks.end(), spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);
        spdlog::register_logger(g_logger);
        g_logger->set_level(spdlog::level::debug);
        g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e%z] [%n] [%^%l%$] [thread %t] %v");
    }
    catch (const std::exception& e)
    {
        std::cout << std::format("failed to set up logging: {}\n", e.what());
        return EXIT_FAILURE;
    }

    try
    {
        boost::asio::io_context io_context(1);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           {
                               g_logger->info("exiting");
                               io_context.stop();
                           });

        co_spawn(io_context, listener(), detached);

        io_context.run();
    }
    catch (const std::exception& e)
    {
        g_logger->error("error: {}", e.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        g_logger->error("unknown error");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
