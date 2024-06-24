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
 */

#ifndef USCRIPT_MSGBUF_UMB_HPP
#define USCRIPT_MSGBUF_UMB_HPP

#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define UMB_WINDOWS 1
#else
#define UMB_WINDOWS 0
#endif

#ifdef _MSC_VER
#define UMB_SUPPRESS_MSVC_WARNING(n) __pragma(warning(suppress : n))
#define UMB_WARNING_PUSH_MSVC __pragma(warning(push))
#define UMB_WARNING_POP_MSVC __pragma(warning(pop))
#define UMB_WARNING_DISABLE_MSVC(n) __pragma(warning(disable: n))
#else
#define UMB_SUPPRESS_MSVC_WARNING(n)
#define UMB_WARNING_PUSH_MSVC
#define UMB_WARNING_POP_MSVC
#define UMB_WARNING_DISABLE_MSVC(n)
#endif

#include "umb/coding.hpp"
#include "umb/constants.hpp"
#include "umb/floatcmp.hpp"
#include "umb/fmt.hpp"
#include "umb/message.hpp"

#ifdef UMB_INCLUDE_META

#include "umb/meta.hpp"
#include "umb/meta_rng.hpp"

#endif

#endif // USCRIPT_MSGBUF_UMB_HPP
