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
 * Copyright (c) 2021 Phins
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef USCRIPT_MSGBUF_META_RNG_HPP
#define USCRIPT_MSGBUF_META_RNG_HPP

#pragma once

// TODO: put implementation detail headers in subdirectories?

#include <array>
#include <cstdint>
#include <utility>

namespace umb::meta::rng
{

template<
    uint64_t X = 1,
    uint64_t Y = 1,
    uint64_t Z = 1,
    uint64_t W = 1>
class KissBase
{
public:
    using next = KissBase<6906969069ULL * X + 1234567ULL,
        ((Y ^ (Y << 13)) ^ ((Y ^ (Y << 13)) >> 17)) ^
        (((Y ^ (Y << 13)) ^ ((Y ^ (Y << 13)) >> 17)) << 11),
        Z + ((Z << 26) + W),
        ((Z + ((Z << 26) + W)) >> 6) + (Z + ((Z << 26) + W) < ((Z << 26) + W))>;

    constexpr static uint64_t value = X + Y + Z;
};

constexpr uint64_t hash(uint64_t x)
{
    constexpr std::array<uint64_t, 16> table{
        0x7ad870c830358979ULL, 0x8632586aca9a2710ULL,
        0x2e675fb4576761d0ULL, 0xd9810c6891bd5c04ULL,
        0x8f689158505e9b8bULL, 0xb3ba5c8932b0836dULL,
        0x854fb3003f739fa5ULL, 0x2aca3b2d1a053f9dULL,
        0x8464ea266c2b0836ULL, 0x460abd1952db919fULL,
        0xb99d7ed15d9d8743ULL, 0xf5b0e190606b12f2ULL,
        0xe98f353477ad31a7ULL, 0x5fb354025b277b14ULL,
        0x6debdf121b863991ULL, 0x358804e3f82aa47dULL,
    };
    uint64_t out = x ^ table[x & 0xF];
    out ^= std::rotl(x, 20) ^ table[(x >> 10) & 0xF];
    out ^= std::rotl(x, 10) ^ table[(x >> 30) & 0xF];
    out ^= std::rotr(x, 10) ^ table[(x >> 60) & 0xF];
    return out;
}

constexpr uint64_t time_seed()
{
    return hash(__TIME__[0]) ^ hash(__TIME__[1]) ^
           hash(__TIME__[2]) ^ hash(__TIME__[3]) ^
           hash(__TIME__[4]) ^ hash(__TIME__[5]) ^
           hash(__TIME__[6]) ^ hash(__TIME__[7]);
}

constexpr uint64_t kiss_seed = time_seed();

template<uint64_t Iteration, uint64_t Seed>
class Kiss
{
private:
    friend Kiss<Iteration + 1, Seed>;
    using current = Kiss<Iteration - 1, Seed>::current::next;

public:
    constexpr static uint64_t value = current::value;
};

template<uint64_t Seed>
class Kiss<0ULL, Seed>
{
private:
    friend Kiss<1ULL, Seed>;

    using current = KissBase<
        Seed ^ 1066149217761810ULL,
        Seed ^ 362436362436362436ULL,
        Seed ^ 1234567890987654321ULL,
        Seed ^ 123456123456123456ULL>::next;

public:
    constexpr static uint64_t value = current::value;
};

template<uint64_t Iteration>
constexpr static uint64_t KissValue = Kiss<Iteration, kiss_seed>::value;

} // namespace umb::meta::rng

#endif // USCRIPT_MSGBUF_META_RNG_HPP
