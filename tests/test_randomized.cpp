#ifndef __JETBRAINS_IDE__
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#endif

#include <algorithm>
#include <cstdint>

#include <doctest/doctest.h>

#include <boost/hana.hpp>
#include <boost/hana/ext/std/array.hpp>

#include "umb/umb.hpp"

#include "MoreMessage.umb.hpp"
#include "TestMessages.umb.hpp"

TEST_CASE("test TestMessages messages with randomized data")
{
    // // Yea...
    // for (constexpr auto mt = ::testmessages::umb::meta::first_message_type();
    //      static_cast<uint16_t>(mt) <
    //      static_cast<uint16_t>(::testmessages::umb::meta::last_message_type());
    //      mt = static_cast<::testmessages::umb::MessageType>(static_cast<uint16_t>(mt) + 1))
    // {
    //     testmessages::umb::meta::Message<mt>::
    // }

    // constexpr auto mts = boost::hana::make_tuple(::testmessages::umb::meta::message_types());
//    boost::hana::for_each(mts, [](const auto mt)
//    {
//        testmessages::umb::meta::Message<mt>::field_count();
//    });

    constexpr auto mts = ::testmessages::umb::meta::message_types();

    constexpr auto temp = ::testmessages::umb::meta::Message<::testmessages::umb::MessageType::GetSomeStuffResp>::field_count();
//    for (size_t i = 0; i < mts.size(); ++i)
//    {
//        testmessages::umb::meta::Message<i>::field_count();
//        // boost::hana::at_c<2>(mts);
//    }
    // const std::array<int, 1> xs = {{5}};
    boost::hana::unpack(mts, [](const auto... mt)
    {
        ([&]
        {
            // const auto temp = mt;
            constexpr auto temp = testmessages::umb::meta::Message<mt>::field_count();
        }(), ...);
    });

//    ([&]
//    {
//        testmessages::umb::meta::Message<mts>::field_count();
//    }(), ...);
}
