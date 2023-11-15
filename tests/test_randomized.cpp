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
 */

#ifndef __JETBRAINS_IDE__
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#endif

#include <doctest/doctest.h>

#include <iostream>

// Only possible with reflection.
#ifdef UMB_INCLUDE_META

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

#include <boost/hana.hpp>

#include <unicode/unistr.h>
#include <unicode/ustream.h>

#include "umb/umb.hpp"

#include "MoreMessage.umb.hpp"
#include "TestMessages.umb.hpp"

// TODO: include this in top level CMake as an option.
// Warning: going too high will hit the stack limit.
#ifndef UMB_META_TEST_ROUNDS
#define UMB_META_TEST_ROUNDS 16
#endif

namespace
{

template<uint64_t X, uint64_t Y, uint64_t Z, uint64_t W = 1>
constexpr uint64_t get_rand()
{
    constexpr auto counter = __COUNTER__ + X + Y + Z + W;
    return ::umb::meta::rng::KissValue<counter>;
}

template<
    auto Index,
    auto FIndex,
    auto Round,
    typename IndexType = size_t,
    IndexType... Is>
constexpr std::u16string get_rand_str(std::integer_sequence<IndexType, Is...> slen_seq)
{
    std::u16string str_in;
    str_in.reserve(slen_seq.size());
    boost::hana::for_each(slen_seq, [&](const auto i)
    {
        constexpr auto i64 = static_cast<uint64_t>(i);
        constexpr auto rnd_idx = get_rand<Index, FIndex, Round, i64>();
        str_in.append(1, static_cast<char16_t>(rnd_idx & 0xffff));
    });

    return str_in;
}

template<
    auto Index,
    auto FIndex,
    auto Round,
    typename IndexType = size_t,
    IndexType... Is>
std::vector<::umb::byte> get_rand_bytes(std::integer_sequence<IndexType, Is...> blen_seq)
{
    std::vector<::umb::byte> bytes;
    bytes.reserve(blen_seq.size());
    boost::hana::for_each(blen_seq, [&](const auto i)
    {
        constexpr auto i64 = static_cast<uint64_t>(i);
        constexpr auto rnd_idx = get_rand<Index, FIndex, Round, i64>();
        bytes.emplace_back(static_cast<::umb::byte>(rnd_idx & 0xff));
    });
    return bytes;
}

std::string
bytes_to_string(const std::vector<::umb::byte>& bytes)
{
    std::stringstream ss;
    ss << "[";
    for (const auto& byte: bytes)
    {
        ss << std::format("{:x},", byte);
    }
    ss << "]";
    return ss.str();
}

std::string
byte_cmp_msg(const std::vector<::umb::byte>& b1, const std::vector<::umb::byte>& b2)
{
    std::stringstream ss{"bytes:\n"};
    const auto max_len = std::max(b1.size(), b2.size());
    for (size_t i = 0; i < max_len; ++i)
    {
        const auto e1 = b1.at(i);
        const auto e2 = b2.at(i);
        const auto cmp = e1 == e2 ? "==" : "!=";
        // TODO: determine first field width.
        ss << std::format("  [{:5}] - '{:3}' {} '{:3}'\n", i, e1, cmp, e2);
    }
    return ss.str();
}

}

TEST_CASE("test TestMessages messages with randomized data")
{
    namespace meta = ::testmessages::umb::meta;

    constexpr auto rounds = std::make_integer_sequence<uint64_t, UMB_META_TEST_ROUNDS>();
    constexpr auto mts = meta::message_types();
    constexpr auto seq = std::make_integer_sequence<uint64_t, mts.size()>();

    boost::hana::for_each(rounds, [&](const auto round)
    {
        std::cout << std::format("\n####### round: {}\n", static_cast<uint64_t>(round));

        boost::hana::for_each(seq, [&](const auto index)
        {
            constexpr auto i = static_cast<uint16_t>(index);
            constexpr auto mt = static_cast<::testmessages::umb::MessageType>(i);
            constexpr auto field_count = meta::Message<mt>::field_count();

            // Skip MessageType::None.
            // TODO: is there a constexpr way to specify start index for
            //   std::make_index_sequence to avoid this if branch?
            if constexpr (index > 0)
            {
                std::cout << std::format("index={}, mt={}, field_count={}\n",
                                         std::to_string(index),
                                         meta::to_string<mt>(), field_count);

                std::shared_ptr<::umb::Message> message = meta::make_shared_message<mt>();

                constexpr auto field_seq = std::make_integer_sequence<uint64_t, field_count>();
                boost::hana::for_each(field_seq, [&](const auto findex)
                {
                    constexpr auto field = meta::Message<mt>::template field<findex>();
                    std::cout << std::format("-- field.name={}\n", field.name);

                    constexpr uint64_t r = get_rand<index, findex, round>();
                    std::cout << std::format("-- r={}\n", std::to_string(r));

                    if constexpr (field.type == ::umb::meta::FieldType::Integer)
                    {
                        constexpr auto int_in = static_cast<int32_t>(r & 0xFFFFFFFF);
                        meta::set_field<mt, field.type, field.name, int_in>(message);
                        const auto int_val = meta::get_field<mt, field.type, field.name>(message);
                        std::cout << std::format("-- (Integer) {}={}\n", field.name, int_val);
                        CHECK_EQ(int_in, int_val);
                    }
                    else if constexpr (field.type == ::umb::meta::FieldType::Byte)
                    {
                        constexpr auto byte_in = static_cast<::umb::byte>(r & 0xFF);
                        meta::set_field<mt, field.type, field.name, byte_in>(message);
                        const auto byte_val = meta::get_field<mt, field.type, field.name>(message);
                        std::cout << std::format("-- (Byte) {}={}\n", field.name, byte_val);
                        CHECK_EQ(byte_in, byte_val);
                    }
                    else if constexpr (field.type == ::umb::meta::FieldType::Boolean)
                    {
                        constexpr auto bool_in = r > std::numeric_limits<uint64_t>::max() / 2;
                        meta::set_field<mt, field.type, field.name, bool_in>(message);
                        const auto bool_val = meta::get_field<mt, field.type, field.name>(message);
                        std::cout << std::format("-- (Boolean) {}={}\n", field.name, bool_val);
                        CHECK_EQ(bool_in, bool_val);
                    }
                    else if constexpr (field.type == ::umb::meta::FieldType::Float)
                    {
                        constexpr auto shift = static_cast<uint8_t>(r & 0x7);
                        constexpr auto mask = static_cast<uint8_t>(r & 0xFF);
                        constexpr auto float_in = std::bit_cast<float>(
                            static_cast<uint32_t>(((r & 0xFFFFFFFF) ^ mask) >> shift));
                        meta::set_field<mt, field.type, field.name, float_in>(message);
                        const auto float_val = meta::get_field<mt, field.type, field.name>(message);
                        std::cout << std::format("-- (Float) {}={}\n", field.name, float_val);
                        if constexpr (std::isnan(float_in))
                        {
                            CHECK(std::isnan(float_val));
                        }
                        else
                        {
                            REQUIRE(float_in == doctest::Approx(float_val));
                        }
                    }
                    else if constexpr (field.type == ::umb::meta::FieldType::String)
                    {
                        constexpr auto slen = static_cast<uint8_t>(r & 0xFF);
                        constexpr auto slen_seq = std::make_integer_sequence<uint64_t, slen>();
                        std::cout << std::format("-- DYNAMIC slen: {}\n", slen);
                        const std::u16string str_in = get_rand_str<index, findex, round>(
                            slen_seq);
                        meta::set_field_dynamic<mt, field.type, field.name>(message, str_in);
                        const auto str_val = meta::get_field<mt, field.type, field.name>(message);
                        const auto icu_string = icu::UnicodeString(str_val.data());
                        std::cout << std::format("-- (String) {}=", field.name) << icu_string
                                  << "\n";
                        CHECK_EQ(str_in, str_val);
                    }
                    else if constexpr (field.type == ::umb::meta::FieldType::Bytes)
                    {
                        constexpr auto bshift = static_cast<uint8_t>(r & 0x7);
                        constexpr auto blen = static_cast<uint8_t>((r >> bshift) & 0xFF);
                        constexpr auto blen_seq = std::make_integer_sequence<uint64_t, blen>();
                        std::cout << std::format("-- DYNAMIC blen: {}\n", blen);
                        const std::vector<::umb::byte> bytes_in = get_rand_bytes<index, findex, round>(
                            blen_seq);
                        meta::set_field_dynamic<mt, field.type, field.name>(message, bytes_in);
                        const auto bytes_val = meta::get_field<mt, field.type, field.name>(message);
                        std::cout << std::format("-- (Bytes) {}={}\n", field.name,
                                                 bytes_to_string(bytes_val));
                        CHECK_EQ(bytes_in, bytes_val);
                    }
                    else
                    {
                        static_assert("invalid FieldType");
                    }
                });

                std::shared_ptr<::umb::Message> m2 = meta::make_shared_message<mt>();

                const auto bytes = message->to_bytes();
                CHECK_EQ(bytes.size(), message->serialized_size());
                bool ok = m2->from_bytes(bytes);
                CHECK(ok);
                const auto bytes2 = m2->to_bytes();
                CHECK_MESSAGE(bytes == bytes2, byte_cmp_msg(bytes, bytes2));
                // TODO: meta comparison functions?

                const auto mt1 = message->type();
                const auto mt1_str = ::testmessages::umb::meta::to_string(mt1);
                const auto mt2_str = ::testmessages::umb::meta::to_string(m2->type());
                CHECK_EQ(mt1, static_cast<uint16_t>(mt));
                CHECK_EQ(mt1_str, mt2_str);

                std::cout << std::endl;
            }
        });
    });
}

#else

TEST_CASE("skipping randomized tests")
{
    std::cout << "UMB_INCLUDE_META not set, skipping randomized tests\n";
    CHECK(true);
}

#endif
