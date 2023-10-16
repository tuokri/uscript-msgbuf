#ifdef __clang__
#pragma clang diagnostic push
#endif
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <unicode/unistr.h>
#include <unicode/ustream.h>

#include <doctest/doctest.h>

#include "umb/umb.hpp"

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

TEST_CASE("encode decode message with dynamic data")
{
    testmessages::umb::testmsg msg1;
    testmessages::umb::testmsg msg2;
    msg1.set_ffffff(u"(this is a UTF-16 string with some text 'äsdfä'2ä34 --__@@)");

    auto bytes = msg1.to_bytes();
    CHECK_EQ(bytes.size(), msg1.serialized_size());

    auto ok = msg2.from_bytes(bytes);
    CHECK(ok);

    const auto str1 = msg1.ffffff();
    const auto str2 = msg2.ffffff();
    const auto u16str1 = icu::UnicodeString(str1.data());
    const auto u16str2 = icu::UnicodeString(str2.data());
    CHECK_EQ(u16str1, u16str2);
    CHECK_EQ(msg1, msg2);
    CHECK_EQ(str1, str2);

    testmessages::umb::testmsg msg3;
    testmessages::umb::testmsg msg4;
    msg3.set_a_field_with_some_bytes_that_do_some_things(bytes);

    bytes = msg3.to_bytes();

    CHECK_EQ(bytes.size(), msg3.serialized_size());
    ok = msg4.from_bytes(bytes);
    CHECK(ok);

    CHECK_EQ(msg1, msg2);
    CHECK_EQ(str1, str2);

    CHECK_EQ(msg3.serialized_size(), msg4.serialized_size());

    msg3.set_aa(58484);
    msg3.set_nmmmgfgg234(1);
    CHECK_NE(msg3, msg4);

    bytes = msg3.to_bytes();
    const auto bytes2 = msg4.to_bytes();
    CHECK_NE(bytes, bytes2);
}
