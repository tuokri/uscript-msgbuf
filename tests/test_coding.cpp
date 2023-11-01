#ifndef __JETBRAINS_IDE__
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#endif

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
}
