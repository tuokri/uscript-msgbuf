// doctest.h - the lightest feature-rich C++ single-header testing framework for unit tests and TDD
//
// Copyright (c) 2016-2023 Viktor Kirilov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
// The documentation can be found at the library's page:
// https://github.com/doctest/doctest/blob/master/doc/markdown/readme.md

#ifndef USCRIPT_MSGBUF_FLOATCMP_DOCTEST_HPP
#define USCRIPT_MSGBUF_FLOATCMP_DOCTEST_HPP

#pragma once

#include <cmath>
#include <limits>

#include "umb/constants.hpp"

namespace
{

constexpr float _epsilon = std::numeric_limits<float>::epsilon() * 100;

} // namespace

namespace umb::internal
{

inline UMB_CONSTEXPR bool
approx_equal(float lhs, float rhs, float epsilon = _epsilon)
{
    // Thanks to Richard Harris for his help refining this formula.
    return std::fabs(lhs - rhs) <
           epsilon * std::max<float>(std::fabs(lhs), std::fabs(rhs));
}

} // namespace umb::internal

#endif // USCRIPT_MSGBUF_FLOATCMP_DOCTEST_HPP
