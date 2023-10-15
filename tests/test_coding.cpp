#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#pragma clang diagnostic pop

#include "doctest/doctest.h"

#include "umb/umb.hpp"

#include "TestMessages.umb.hpp"

TEST_CASE("encode decode empty message")
{
    testmessages::umb::GetSomeStuff msg1;
    const auto bytes = msg1.to_bytes();
    testmessages::umb::GetSomeStuff msg2;
    const auto ok = msg2.from_bytes(bytes);
    CHECK(ok);
}
