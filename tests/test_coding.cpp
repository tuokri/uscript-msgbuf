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
#endif

#include <cmath>
#include <limits>

#include <unicode/unistr.h>
#include <unicode/ustream.h>

#include <doctest/doctest.h>

#include "umb/umb.hpp"

#include "MoreMessage.umb.hpp"
#include "TestMessages.umb.hpp"

TEST_CASE("encode decode empty message")
{
    testmessages::umb::GetSomeStuff msg1;
    testmessages::umb::GetSomeStuff msg2;

    std::vector<::umb::byte> bytes = msg1.to_bytes();
    CHECK_EQ(bytes.size(), msg1.serialized_size());

    bool ok = msg2.from_bytes(bytes);
    CHECK(ok);
    CHECK_EQ(msg1, msg2);

    testmessages::umb::testmsg tmsg1;
    testmessages::umb::testmsg tmsg2;

    bytes = tmsg1.to_bytes();
    CHECK_EQ(bytes.size(), tmsg1.serialized_size());

    ok = tmsg2.from_bytes(bytes);
    CHECK(ok);
    CHECK_EQ(tmsg1, tmsg2);
}

TEST_CASE("encode decode empty message from MoreMessages")
{
    moremessages::XGonGetIt xggi1;
    moremessages::XGonGetIt xggi2;

    std::vector<::umb::byte> bytes = xggi1.to_bytes();
    CHECK_EQ(bytes.size(), xggi1.serialized_size());

    bool ok = xggi2.from_bytes(bytes);
    CHECK(ok);
    CHECK_EQ(xggi1, xggi2);
}

TEST_CASE("encode decode message with dynamic string")
{
    testmessages::umb::testmsg msg1;
    testmessages::umb::testmsg msg2;
    msg1.set_ffffff(u"this is a UTF-16 string with some text 'äsdfä'2ä34 --__@@");

    auto bytes = msg1.to_bytes();
    CHECK_EQ(bytes.size(), msg1.serialized_size());

    auto ok = msg2.from_bytes(bytes);
    CHECK(ok);

    auto str1 = msg1.ffffff();
    auto str2 = msg2.ffffff();
    auto u16str1 = icu::UnicodeString(str1.data());
    auto u16str2 = icu::UnicodeString(str2.data());
    CHECK_EQ(u16str1, u16str2);
    CHECK_EQ(msg1, msg2);
    CHECK_EQ(str1, str2);

    // Reset the string and check again.
    msg1.set_ffffff(u"");
    bytes = msg1.to_bytes();
    ok = msg2.from_bytes(bytes);
    CHECK(ok);
    str1 = msg1.ffffff();
    str2 = msg2.ffffff();
    u16str1 = icu::UnicodeString(str1.data());
    u16str2 = icu::UnicodeString(str2.data());
    CHECK_EQ(u16str1, u16str2);
    CHECK_EQ(msg1, msg2);
    CHECK_EQ(str1, str2);
}


TEST_CASE("encode decode long unicode string")
{
    testmessages::umb::testmsg msg1;
    testmessages::umb::testmsg msg2;

    constexpr auto raw_str = u"\u4b9f\uc09d\ubfe3\ubc86\ube5a"
                             "\uc172\uc2a3\u9b33\uc416\ufc4b"
                             "\u51b9\ufa94\uc81d\u8d71\u96f4"
                             "\ub387\u8177\u960e\u4c5d\u37c5"
                             "\u6226\u2ff6\ud70d\u1cb9\u2b4c"
                             "\u8c8d\u00f6\u00e4\u0069\u64f3"
                             "\ua29e\uc89c\ua24b\u1238\u4e13"
                             "\u9f61\ubcf3\ud56f\ud66f\ud197"
                             "\ufb72\u07b1\u8174\u8275\u5683"
                             "\uc39d\u9fae\u66ae\u8454\u3e9a"
                             "\u1dd9\u339a\u683c\u9f96\ua8e5"
                             "\u29e6\u5718\u8564\u00e5\u0061"
                             "\ub5dc\u2ab0\u9ed1\ucf3b\u4621"
                             "\u89aa\u6c0f\u4443\u8410\u89e2"
                             "\u66e0\u8072\u7b53\ua65c\ud355"
                             "\u14c9\u4466\u49fd\uc78f\u746c"
                             "\u7e6c\u02dd\ua3e5\u5c74\u8271"
                             "\ud5ea\u25a1\uacd2\u00d6\u5745"
                             "\ua783\u11f0\u5011\uff70\u81ec"
                             "\u0e57\u5a5e\u3ac9\u0075\u0067"
                             "\u0069\u0066";

    msg1.set_ffffff(raw_str);

    const auto raw_str_icu = icu::UnicodeString(raw_str);
    std::cout << "raw_str (icu)  : " << raw_str_icu << "\n";
    std::cout << "raw_str (size) : " << raw_str_icu.length() << "\n";

    auto bytes = msg1.to_bytes();
    CHECK_EQ(bytes.size(), msg1.serialized_size());

    auto ok = msg2.from_bytes(bytes);
    CHECK(ok);

    auto str1 = msg1.ffffff();
    auto str2 = msg2.ffffff();
    auto u16str1 = icu::UnicodeString(str1.data());
    auto u16str2 = icu::UnicodeString(str2.data());
    CHECK_EQ(u16str1.length(), u16str2.length());
    CHECK_EQ(u16str1, u16str2);
    CHECK_EQ(msg1, msg2);
    CHECK_EQ(str1, str2);
}

TEST_CASE("encode decode message with dynamic bytes")
{
    testmessages::umb::testmsg msg3;
    testmessages::umb::testmsg msg4;

    std::vector<::umb::byte> field_bytes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 0, 0, 0, 5, 55, 255};
    msg3.set_a_field_with_some_bytes_that_do_some_things(field_bytes);

    auto bytes = msg3.to_bytes();

    CHECK_EQ(bytes.size(), msg3.serialized_size());
    bool ok = msg4.from_bytes(bytes);
    CHECK(ok);

    auto msg4_bytes = msg4.to_bytes();

    CHECK_EQ(bytes, msg4_bytes);
    CHECK_EQ(msg3, msg4);
    CHECK_EQ(msg3.serialized_size(), msg4.serialized_size());

    msg3.set_aa(58484);
    msg3.set_nmmmgfgg234(1);
    CHECK_NE(msg3, msg4);

    bytes = msg3.to_bytes();
    const auto bytes2 = msg4.to_bytes();
    CHECK_NE(bytes, bytes2);
}

TEST_CASE("encode decode boolean packing message")
{
    testmessages::umb::BoolPackingMessage bpm1;
    testmessages::umb::BoolPackingMessage bpm2;

    bpm1.set_bool_10_part_pack__0(false);
    bpm1.set_bool_10_part_pack__5(false);
    bpm1.set_pack2(false);
    bpm1.set_pack1(true);
    bpm1.set_int_pack_delimiter(69);
    bpm1.set_end_msg_with_some_dynamic_stuff(u"(asd)");

    std::vector<::umb::byte> vec;
    const auto ss = bpm1.serialized_size();
    vec.resize(ss);
    bool ok = bpm1.to_bytes(vec);
    const auto vec2 = bpm1.to_bytes();
    CHECK(ok);
    CHECK_EQ(vec, vec2);

    ok = bpm2.from_bytes(vec);
    CHECK(ok);
    CHECK_EQ(bpm1, bpm2);
}

TEST_CASE("encode decode static boolean packing message")
{
    testmessages::umb::STATIC_BoolPackingMessage sbpm1;
    testmessages::umb::STATIC_BoolPackingMessage sbpm2;

    sbpm1.set_bool_10_part_pack__0(false);
    sbpm1.set_bool_10_part_pack__5(false);
    sbpm1.set_bool_10_part_pack__7(true);
    sbpm1.set_bool_10_part_pack__9(true);
    sbpm1.set_pack2(false);
    sbpm1.set_pack1(true);
    sbpm1.set_int_pack_delimiter(69);

    std::vector<::umb::byte> vec;
    const auto ss = sbpm1.serialized_size();
    vec.resize(ss);
    bool ok = sbpm1.to_bytes(vec);
    const auto vec2 = sbpm1.to_bytes();
    CHECK(ok);
    CHECK_EQ(vec, vec2);

    ok = sbpm2.from_bytes(vec);
    CHECK(ok);
    CHECK_EQ(sbpm1, sbpm2);

    CHECK_EQ(sbpm1.bool_10_part_pack__0(), false);
    CHECK_EQ(sbpm2.bool_10_part_pack__0(), false);
    CHECK_EQ(sbpm1.bool_10_part_pack__5(), false);
    CHECK_EQ(sbpm2.bool_10_part_pack__5(), false);
    CHECK_EQ(sbpm1.bool_10_part_pack__7(), true);
    CHECK_EQ(sbpm2.bool_10_part_pack__7(), true);
    CHECK_EQ(sbpm1.bool_10_part_pack__9(), true);
    CHECK_EQ(sbpm2.bool_10_part_pack__9(), true);
}

TEST_CASE("encode decode float fields")
{
    using Float = ::umb::internal::Float;

    testmessages::umb::JustAnotherTestMessage jatm1;
    testmessages::umb::JustAnotherTestMessage jatm2;

    const float ones = 1.11111111111111F;
    jatm1.set_some_floatVAR(ones);
    std::vector<::umb::byte> vec = jatm1.to_bytes();
    bool ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(std::numeric_limits<float>::max());
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(std::numeric_limits<float>::min());
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(0.0);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(0.1F);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(35848.9858405F);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(0.00583885F);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(-0.00583885F);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(-5885.000838869695000F);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(static_cast<float>(-8553588573958e-27));
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(-0.0);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(static_cast<float>(-10101011100001e-40));
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    // Doesn't work for extremely small negative floats.
    // CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(static_cast<float>(-3.828379e-38));
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    // Doesn't work for extremely small negative floats.
    // CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(0.300000011920928955078125);
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(static_cast<float>(3.8571562e-30));
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    REQUIRE(jatm1.some_floatVAR() == doctest::Approx(jatm2.some_floatVAR()));
    CHECK(Float(jatm1.some_floatVAR()).AlmostEquals(Float(jatm2.some_floatVAR())));
    CHECK_EQ(jatm1, jatm2);
}

TEST_CASE("encode decode float inf/nan fields")
{
    testmessages::umb::JustAnotherTestMessage jatm1;
    testmessages::umb::JustAnotherTestMessage jatm2;

    jatm1.set_some_floatVAR(std::numeric_limits<float>::infinity());
    std::vector<::umb::byte> vec = jatm1.to_bytes();
    bool ok = jatm2.from_bytes(vec);
    CHECK(ok);

    CHECK(std::isinf(jatm1.some_floatVAR()));
    CHECK(std::isinf(jatm2.some_floatVAR()));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(-std::numeric_limits<float>::infinity());
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    CHECK(std::isinf(jatm1.some_floatVAR()));
    CHECK(std::isinf(jatm2.some_floatVAR()));
    CHECK_EQ(jatm1, jatm2);

    jatm1.set_some_floatVAR(std::nanf("0"));
    vec = jatm1.to_bytes();
    ok = jatm2.from_bytes(vec);
    CHECK(ok);

    CHECK(std::isnan(jatm1.some_floatVAR()));
    CHECK(std::isnan(jatm2.some_floatVAR()));
    // NOTE: Comparing nan to anything is always false.
    // However, for UMB message comparison, we want to
    // return true if both messages have the field as nan!
    CHECK_EQ(jatm1, jatm2);
}

TEST_CASE("shared pointer testmsg")
{
    std::vector<umb::byte> msg_buf;
    // Empty buffer.
    auto msg = std::make_shared<testmessages::umb::testmsg>();
    bool ok = msg->from_bytes(msg_buf);
    // Should fail.
    CHECK_FALSE(ok);

    msg_buf = {33, 54, 52, 56, 53, 51, 55, 54, 52, 49, 54, 51, 53, 54, 53, 50, 53, 50, 48, 48, 48,
               48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 34, 49, 49, 53, 49, 48, 54, 51,
               52, 56, 57, 53, 49, 52, 49, 54, 49, 57, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
               48, 46, 48, 48, 48, 48, 34, 50, 56, 53, 49, 54, 57, 54, 51, 57, 50, 52, 56, 51, 55,
               56, 57, 50, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 34,
               49, 54, 50, 56, 53, 54, 49, 54, 56, 48, 52, 55, 55, 49, 55, 55, 55, 48, 48, 48, 48,
               48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 34, 50, 52, 57, 51, 50, 48, 57,
               50, 55, 52, 54, 49, 54, 50, 57, 57, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
               48, 46, 48, 48, 48, 48, 33, 53, 51, 53, 54, 53, 51, 54, 54, 49, 57, 54, 50, 57, 48,
               55, 50, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 34, 50,
               51, 49, 49, 48, 53, 56, 49, 53, 52, 54, 51, 57, 48, 53, 49, 48, 48, 48, 48, 48, 48,
               48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 33, 53, 50, 56, 0, 0, 0, 0, 54, 57,
               53, 55, 56, 54, 50, 49, 54, 54, 50, 56, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
               48, 48, 46, 48, 48, 48, 48, 34, 50, 55, 55, 50, 55, 55, 49, 50, 52, 53, 51, 51, 55,
               50, 54, 52, 55, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48,
               34, 49, 52, 55, 57, 51, 51, 48, 53, 51, 49, 52, 50, 57, 53, 48, 48, 51, 48, 48, 48,
               48, 48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48, 34, 50, 51, 53, 51, 56, 52,
               51, 55, 51, 57, 52, 52, 55, 49, 52, 54, 54, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
               48, 48, 46, 48, 48, 48, 48, 34, 49, 49, 52, 50, 56, 53, 57, 52, 52, 48, 50, 56, 50,
               54, 51, 52, 54, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 46, 48, 48, 48, 48,
               92, 0, 0, 32, 37, 190, 234, 124, 58, 11, 142, 39, 170, 140, 112, 127, 184, 207, 104,
               56, 162, 247, 126, 60, 48, 27, 162, 88, 154, 239, 36, 41, 18, 60, 2, 20, 224, 235,
               152, 15, 90, 195, 188, 104, 126, 94, 226, 211, 144, 214, 86, 241, 124, 214, 4, 45,
               84, 164, 18, 167, 176, 88, 158, 186, 96, 195, 220, 228, 2, 247, 26, 95, 26, 196, 168,
               210, 52, 0, 0, 0, 046, 80, 70, 148, 188, 92, 225, 42, 18, 182, 128, 24, 23, 64, 127,
               178, 208, 250, 154, 44, 107, 222, 172, 208, 146, 60, 26, 28, 169, 110, 154, 126, 68,
               164, 68, 8, 96, 184, 245, 252, 74, 154, 30, 96, 102, 132, 16, 144, 177, 96, 4, 246,
               172, 130, 135, 218, 212, 70, 200, 132, 132, 166, 145, 140, 20, 72, 189, 172, 29, 32,
               160, 194, 147, 60, 150, 142, 139, 186, 118, 188, 143, 124, 129, 196, 16, 192, 100,
               50, 85, 4, 105, 240, 254, 110, 198, 20, 92, 38, 191, 194, 240, 210, 118, 210, 145,
               168, 191, 130, 166, 56, 90, 42, 90, 118, 141, 20, 77, 18, 169, 108, 78, 192, 20, 132,
               119, 92, 35, 66, 67, 166, 25, 14, 147, 132, 212, 250, 156, 102, 91, 160, 29, 220,
               143, 72, 122, 248, 51, 234, 102, 116, 205, 152, 189, 66, 156, 228, 35, 140, 15, 206,
               124, 182, 252, 208, 156, 116, 120, 144, 223, 128, 178, 200, 189, 254, 184, 12, 244,
               46, 177, 200, 209, 96, 255, 74, 130, 214, 40, 46, 48, 150, 14, 230, 101, 188, 152,
               202, 164, 90, 38, 98, 118, 128, 54, 94, 218, 122, 227, 230, 133, 192, 17, 176, 143,
               206, 192, 148, 126, 68, 220, 8, 223, 152, 96, 62, 38, 164, 190, 62, 96, 110, 70, 196,
               144, 112, 33, 68, 109, 166, 41, 202, 200, 218, 210, 34, 81, 56, 136, 0, 0, 0, 0, 4,
               183, 218, 112, 144, 169, 40, 162, 8, 208, 216, 127, 240, 194, 110, 133, 138, 144,
               220, 21, 218, 54, 96, 82, 38, 80, 36, 130, 36, 149, 154, 96, 176, 114, 92, 217, 108,
               95, 98, 108, 244, 127, 66, 206, 32, 31, 146, 116, 2, 42, 202, 190, 2, 222, 2, 113,
               240, 201, 200, 93, 52, 201, 137, 91, 225, 70, 244, 125, 28, 69, 41, 132, 111, 16,
               133, 30, 244, 105, 22, 156, 203, 60, 173, 89, 252, 165, 222, 87, 196, 161, 27, 145,
               197, 32, 144, 5, 245, 210, 229, 17, 195, 108, 146, 17, 66, 171, 237, 222, 223, 230,
               148, 38, 116, 116, 103, 42, 35, 167, 168, 238, 245, 175, 127, 95, 132, 35, 116, 115,
               179, 128, 27, 214, 210, 96, 136, 33, 20, 181, 194, 38, 196, 206, 212, 99, 26, 110,
               164, 18, 36, 193, 62, 235, 156, 106, 183, 31, 31, 138, 81, 141, 166, 235, 4, 194,
               174, 48, 26, 195, 244, 67, 7, 50, 39, 203, 24, 205, 142, 250, 55, 216, 231, 118, 82,
               114, 130, 214, 40, 46};
    msg = std::make_shared<testmessages::umb::testmsg>();
    ok = msg->from_bytes(msg_buf);
    // Should fail.
    CHECK(ok);
}
