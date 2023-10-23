#ifndef USCRIPT_MSGBUF_CONSTANTS_HPP
#define USCRIPT_MSGBUF_CONSTANTS_HPP

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace umb
{

using byte = uint8_t;

constexpr auto g_template_dir = "templates";
constexpr auto g_cpp_hdr_extension = ".umb.hpp";
constexpr auto g_cpp_src_extension = ".umb.cpp";
constexpr auto g_cpp_hdr_template = "cpp_header.jinja";
constexpr auto g_cpp_src_template = "cpp_source.jinja";
constexpr auto g_uscript_template = "uscript.jinja";
constexpr auto g_uscript_message_handler_template = "uscript_message_handler.jinja";

constexpr auto g_default_clang_format_config = ".umb.output.clang-format";

// Packet header size in bytes.
constexpr size_t g_header_size = 4;
// Maximum packet payload size in bytes.
constexpr size_t g_payload_size = 251;
// Total size of the packet.
constexpr size_t g_packet_size = g_header_size + g_payload_size;
// Size of the size field of dynamic fields in bytes.
// E.g. the size header field in front of dynamic string fields.
constexpr size_t g_dynamic_field_header_size = 1;
// How many bools can be packed in a byte.
constexpr size_t g_bools_in_byte = 8;
// Maximum number of different message types.
// 2 bytes - the reserved None message.
constexpr uint16_t g_max_message_count = std::numeric_limits<uint16_t>::max() - 1;

// Max size of dynamic field payload part.
constexpr auto g_max_dynamic_size = 255;

constexpr size_t g_sizeof_byte = 1;
constexpr size_t g_sizeof_int32 = 4;
constexpr size_t g_sizeof_uint16 = 2;
// Floats are encoded as int32+int24 (integer part + fractional part).
// Maximum value for the fractional part is 9999999.
// TODO: float fix: actually, this is shit. We have to use strings for floats.
// constexpr size_t g_sizeof_float = 7;
constexpr size_t g_sizeof_uscript_char = 2;

// All types with known static sizes in bytes.
static const std::unordered_map<std::string, size_t> g_static_types{
    {"byte",  g_sizeof_byte},
    {"int",   g_sizeof_int32},
    // TODO: float fix: {"float", g_sizeof_float},
    // NOTE: consecutive bools are packed as bit fields in bytes, thus
    //       the size of a bool in the packet can vary. However, it will
    //       always be known at message generation time.
    {"bool",  g_sizeof_byte},
};

// All dynamic types with variable size i.e. header + data.
static const std::vector<std::string> g_dynamic_types{
    "string",
    "bytes",
};

static const std::unordered_map<std::string, std::string> g_type_to_cpp_type{
    {"byte",   "::umb::byte"},
    {"int",    "int32_t"},
    {"float",  "float"},
    {"bool",   "bool"},
    {"bytes",  "std::vector<::umb::byte>"},
    {"string", "std::u16string"},
};

static const std::unordered_map<std::string, std::string> g_type_to_cpp_type_arg{
    {"byte",   "::umb::byte"},
    {"int",    "int32_t"},
    {"float",  "float"},
    {"bool",   "bool"},
    {"bytes",  "const std::vector<::umb::byte>&"},
    {"string", "const std::u16string&"},
};

static const std::unordered_map<std::string, std::string> g_cpp_default_value{
    {"byte",   "0"},
    {"int",    "0"},
    {"float",  "0"},
    {"bool",   "false"},
    {"bytes",  ""},
    {"string", ""},
};

static const std::unordered_map<std::string, std::string> g_type_to_uscript_type{
    {"bytes", "array<byte>"},
};

} // namespace umb

#endif // USCRIPT_MSGBUF_CONSTANTS_HPP
