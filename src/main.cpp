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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

// Silence "Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately".
#include <SDKDDKVer.h>

#endif

#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <inja/inja.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/process/v2.hpp>
#include <boost/program_options.hpp>

#include "umb/umb.hpp"
// Always include meta explicitly, needed by the generator
// even if not doing a meta build for generated messages.
#include "umb/meta.hpp"

namespace
{

#ifdef UMB_INCLUDE_META
constexpr bool g_generate_meta_cpp = true;
#else
constexpr bool g_generate_meta_cpp = false;
#endif

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::process::v2;

// Utility class for setting and getting global Inja
// variables and passing them to and from included templates.
class Var
{
public:
    Var(std::function<std::any(void)> getter,
        std::function<std::any(std::any)> setter)
        : get(std::move(getter)), set(std::move(setter))
    {
    }

    std::function<std::any(void)> get;
    std::function<std::any(std::any)> set;
};

constexpr auto g_key_x = "x";
constexpr auto g_key_in_pack = "in_pack";
std::unordered_map<std::string, std::any> g_var_store{
    {g_key_x,       0},
    {g_key_in_pack, false},
};

// Inja does not support Jinja macros so let's use these to
// pass variables in and out of include blocks.
std::unordered_map<std::string, Var> g_variables{
    {g_key_x,
        {
            []()
            { return std::any_cast<int>(::g_var_store.at(g_key_x)); },
            [](std::any v)
            {
                ::g_var_store.at(g_key_x) = std::any_cast<int>(std::move(v));
                return ::g_var_store.at(g_key_x);
            }
        }
    },
    {g_key_in_pack,
        {
            []()
            { return std::any_cast<bool>(::g_var_store.at(g_key_in_pack)); },
            [](std::any v)
            {
                ::g_var_store.at(g_key_in_pack) = std::any_cast<bool>(std::move(v));
                return std::any_cast<bool>(::g_var_store.at(g_key_in_pack));
            }
        }
    },
};

enum class VarAction
{
    GET = 0,
    SET = 1,
};

const std::unordered_map<std::string, VarAction> g_str_to_varaction{
    {"GET", VarAction::GET},
    {"SET", VarAction::SET},
};

struct BoolPack
{
    // Name of this field in the message.
    std::string field_name{};
    // Index of this field in the message.
    size_t field_index{0};
    // Index into the packed byte. 0-7.
    ::umb::byte pack_index{0};
    // Indicates which byte this bool belongs to.
    // For this value, lone bools are also counted, thus
    // e.g. a message that has 2 bool packs that are both
    // 1 byte wide, but with a lone unpacked bool between
    // them somewhere, the value for the final pack is 2.
    ::umb::byte byte{0};
    // True if this is the last bool a packed byte.
    bool last{false};
    // True if this bool is the final bool of a byte pack.
    // E.g. for a 3-byte pack with 24 bools, true for all `B`:
    // [bbbbbbbB|bbbbbbbB|bbbbbbbB]
    bool boundary{false};
};

struct MsgAnalysisResult
{
    friend std::ostream& operator<<(std::ostream& os, const MsgAnalysisResult& result);

    // Message name as declared in the input data.
    std::string name{};
    // True if message static_size value was calculated. False for messages
    // with dynamically sized fields or unknown size fields.
    bool has_static_size{false};
    // Message's calculated static size. Sum of header size + static field sizes.
    // Always 0 if message is dynamic. Always non-zero if message is static.
    size_t static_size{0};
    // Size of the static part for dynamic messages. 0 for fully static messages.
    // This includes the sizes of all known static fields plus the sizes of all
    // size header fields for dynamic fields. (See: g_dynamic_field_header_size).
    size_t static_part{0};
    // True if message has static size and is always guaranteed to fit in a single packet.
    bool always_single_part{false};
    // True if message has float fields. Indicates the need for temporary
    // helper variables for decoding and encoding in UnrealScript.
    bool has_float_fields{false};
    // True if message has string fields. Indicates the need for temporary
    // helper variables for decoding and encoding in UnrealScript.
    bool has_string_fields{false};
    // True if message has bytes fields. Indicates the need for temporary
    // helper variables for decoding and encoding in UnrealScript.
    bool has_bytes_fields{false};
    // Hints for packing consecutive boolean fields into byte bitfields.
    std::vector<BoolPack> bool_packs{};
};

std::ostream& operator<<(std::ostream& os, const MsgAnalysisResult& result)
{
    os << "{"
       << " name: " << result.name
       << ", has_static_size: " << result.has_static_size
       << ", static_size: " << result.static_size
       << ", static_part: " << result.static_part
       << ", always_single_part: " << result.always_single_part
       << ", has_float_fields: " << result.has_float_fields
       << ", has_string_fields: " << result.has_string_fields
       << ", has_bytes_fields: " << result.has_bytes_fields
       << " }";
    return os;
}

template<typename T>
inline constexpr bool in_vector(const std::vector<T>& v, T t)
{
    return std::find(v.cbegin(), v.cend(), t) != v.cend();
}

MsgAnalysisResult analyze_message(const inja::json& data)
{
    MsgAnalysisResult result;

    result.name = data["name"];

    const auto& fields = data["fields"];

    std::deque<BoolPack> bool_packs;
    auto consecutive = 0; // Length of current consecutive bool pack.
    auto consecutive_all = 0; // Total length of all bool packs AND "LONE" BOOLS.
    ::umb::byte pack_idx = 0; // Index of this bool in this pack i.e. nth bit.
    auto i = 0;
    for (const auto& field: fields)
    {
        if (field["type"] == "bool")
        {
            BoolPack bp;
            bp.field_name = field["name"];
            bp.field_index = i;
            bp.pack_index = pack_idx;
            bp.byte = static_cast<::umb::byte>(std::floor(
                consecutive_all / ::umb::g_bools_in_byte));

            ++consecutive;
            pack_idx = (pack_idx + 1) % static_cast<::umb::byte>(::umb::g_bools_in_byte);

            // We always know this is the last bool of a pack.
            if ((consecutive % ::umb::g_bools_in_byte) == 0)
            {
                bp.boundary = true;
            }

            bool_packs.emplace_back(bp);
        }
        else
        {
            // Jump by entire pack length, even if the pack was not
            // really a pack, or if the pack wasn't fully filled by
            // booleans. This is so we can get the total size of all
            // booleans used in the message, including the wasted space
            // used by lone bools or partial boolean packs.
            consecutive_all += ::umb::g_bools_in_byte;

            // Started a bool pack "range" earlier, but it turned
            // out to be a single bool. Drop it.
            if (consecutive == 1)
            {
                bool_packs.pop_back();
            }
            else if (consecutive > 1)
            {
                bool_packs.back().last = true;
            }
            consecutive = 0;
            pack_idx = 0;
        }
        ++i;
    }
    // Ensure trailing bool pack's final bool is marked as final.
    if (!bool_packs.empty())
    {
        bool_packs.back().last = true;
    }
    result.bool_packs.reserve(bool_packs.size());
    result.bool_packs.insert(result.bool_packs.end(), bool_packs.cbegin(), bool_packs.cend());

    const auto max_byte_bp = std::max_element(
        bool_packs.cbegin(), bool_packs.cend(),
        [](const BoolPack& first, const BoolPack& second)
        {
            return first.byte < second.byte;
        });
    auto num_packed_bytes = 0;
    if (max_byte_bp != bool_packs.cend())
    {
        // Byte refers to index here, add 1 to get total amount of them.
        num_packed_bytes = max_byte_bp->byte + 1;
    }
    const auto total_pack_size = num_packed_bytes * ::umb::g_sizeof_byte;

    std::vector<std::string> types;
    types.reserve(fields.size());
    std::transform(fields.cbegin(), fields.cend(), std::back_inserter(types), [](const auto& field)
    {
        return field["type"];
    });

    result.has_static_size = std::all_of(types.cbegin(), types.cend(), [](const std::string& type)
    {
        return ::umb::g_static_types.contains(type);
    });

    auto static_size = ::umb::g_header_size + total_pack_size;
    for (const auto& type: types)
    {
        // Total size of all bools is included in total_pack_size.
        if (type != "bool" && ::umb::g_static_types.contains(type))
        {
            static_size += ::umb::g_static_types.at(type);
        }
    }

    if (result.has_static_size)
    {
        result.static_size = static_size;
    }
    else
    {
        result.static_part = static_size;
        for (const auto& type: types)
        {
            if (in_vector(::umb::g_dynamic_types, type))
            {
                result.static_part += ::umb::g_dynamic_field_header_size;
            }
        }
    }

    if (result.has_static_size && result.static_size <= ::umb::g_packet_size)
    {
        result.always_single_part = true;
    }

    result.has_float_fields = std::any_of(types.cbegin(), types.cend(), [](const std::string& type)
    {
        return type == "float";
    });

    result.has_string_fields = std::any_of(types.cbegin(), types.cend(), [](const std::string& type)
    {
        return type == "string";
    });

    result.has_bytes_fields = std::any_of(types.cbegin(), types.cend(), [](const std::string& type)
    {
        return type == "bytes";
    });

    return result;
}

constexpr auto capitalize = [](const inja::Arguments& args)
{
    auto str = args.at(0)->get<std::string>();
    str.at(0) = static_cast<char>(std::toupper(str.at(0)));
    return str;
};

// Calculates padding for index (arg 0) up to
// max-padding (arg 1). Can be used to align columns e.g.:
// Bytes[  0] // index = 0
// Bytes[  1] // index = 1
// ...
// Bytes[ 10]
// ...
// Bytes[101]
constexpr auto pad = [](const inja::Arguments& args)
{
    const auto& index = args.at(0)->get<size_t>();
    const auto& size = args.at(1)->get<size_t>();
    std::string ret;

    if (size < 10)
    {
        return ret;
    }

    const auto max_pad = std::to_string(size).size();
    const auto pad_count = max_pad - std::to_string(index).size();

    std::stringstream ss;
    for (unsigned i = 0; i < pad_count; ++i)
    {
        ss << " ";
    }
    ret = ss.str();
    return ret;
};

// Get and set global variables. Workaround for lack of
// Jinja macro support in Inja. Can be used to pass variables
// in and out of included templates.
template<typename T>
constexpr T var(const inja::Arguments& args)
{
    const auto& var_name = args.at(0)->get<std::string>();
    auto action = args.at(1)->get<std::string>();

    boost::algorithm::to_upper(action);
    VarAction va = g_str_to_varaction.at(action);

    T value;

    switch (va)
    {
        case VarAction::GET:
            return std::any_cast<T>(::g_variables.at(var_name).get());
        case VarAction::SET:
            if (!args.at(2))
            {
                throw std::invalid_argument(std::format("no value to set for '{}'", var_name));
            }
            value = static_cast<T>(*args.at(2));
            ::g_variables.at(var_name).set(value);
            return value;
        default:
            throw std::invalid_argument(std::format("invalid VarAction: {}", action));
    }
}

constexpr auto var_bool = [](const inja::Arguments& args)
{
    return var<bool>(args);
};

constexpr auto var_int = [](const inja::Arguments& args)
{
    return var<int>(args);
};

constexpr auto error = [](const inja::Arguments& args)
{
    std::stringstream ss;
    for (const auto& arg: args)
    {
        ss << arg->get<std::string>();
    }
    throw std::invalid_argument(ss.str());
};

constexpr auto cpp_type = [](const inja::Arguments& args)
{
    const auto& type = args.at(0)->get<std::string>();
    return ::umb::g_type_to_cpp_type.at(type);
};

constexpr auto cpp_type_arg = [](const inja::Arguments& args)
{
    const auto& type = args.at(0)->get<std::string>();
    return ::umb::g_type_to_cpp_type_arg.at(type);
};

constexpr auto cpp_default_value = [](const inja::Arguments& args)
{
    const auto& type = args.at(0)->get<std::string>();
    return ::umb::g_cpp_default_value.at(type);
};

constexpr auto uscript_type = [](const inja::Arguments& args)
{
    const auto& type = args.at(0)->get<std::string>();
    if (!::umb::g_type_to_uscript_type.contains(type))
    {
        return type;
    }
    return ::umb::g_type_to_uscript_type.at(type);
};

constexpr auto bp_is_packed = [](const inja::Arguments& args)
{
    const auto& msg = args.at(0)->get<inja::json>();
    const auto& name = args.at(1)->get<std::string>();
    const auto& bps = msg["bool_packs"].get<std::vector<inja::json>>();

    return std::any_of(bps.cbegin(), bps.cend(), [&name](const inja::json& bp)
    {
        return name == bp["field_name"].get<std::string>();
    });
};

template<typename T>
T bp_get(const inja::json& bps, const std::string& field_name, const std::string& key)
{
    for (const auto& bp: bps)
    {
        if (bp["field_name"] == field_name)
        {
            return bp[key].get<T>();
        }
    }
    throw std::invalid_argument(std::format("cannot find '{}' in '{}'", key, field_name));
}

constexpr auto bp_pack_index = [](const inja::Arguments& args)
{
    const auto& bps = args.at(0)->get<std::vector<inja::json>>();
    const auto& name = args.at(1)->get<std::string>();
    return bp_get<int>(bps, name, "pack_index");
};

constexpr auto bp_is_last = [](const inja::Arguments& args)
{
    const auto& bps = args.at(0)->get<std::vector<inja::json>>();
    const auto& name = args.at(1)->get<std::string>();
    return bp_get<bool>(bps, name, "last");
};

constexpr auto bp_is_multi_pack_boundary = [](const inja::Arguments& args)
{
    const auto& bps = args.at(0)->get<std::vector<inja::json>>();
    const auto& name = args.at(1)->get<std::string>();
    return bp_get<bool>(bps, name, "boundary");
};

// TODO: don't call this from Inja if not generating meta code?
constexpr auto meta_field_type = [](const inja::Arguments& args)
{
    // UMB type string -> FieldType -> FieldType as string.
    const auto& type = args.at(0)->get<std::string>();
    ::umb::meta::FieldType ft = ::umb::meta::from_type_string(type);
    return ::umb::meta::to_string(ft);
};

// TODO: take in string args as fs::paths?
void render_uscript(inja::Environment& env, const std::string& file, const inja::json& data,
                    const std::string& filename, const std::string& uscript_out_dir)
{
    const auto r = env.render_file(file, data);
    fs::path out_filename = fs::path{filename};
    fs::path out_file = fs::path{uscript_out_dir} / out_filename;
    std::cout << std::format("writing '{}'\n", out_file.string());
    fs::ofstream out{out_file};
    out << r;
}

// TODO: add option for passing in custom .clang-format file.
void clang_format(const std::vector<fs::path>& paths)
{
    const auto cf_prog = bp::environment::find_executable("clang-format");
    if (cf_prog.empty())
    {
        std::cout << "clang-format not found, not formatting generated C++ code\n";
        return;
    }

    std::cout << std::format("using clang-format from: '{}'\n", cf_prog.string());

    const auto prog_dir = boost::dll::program_location().parent_path();
    const auto cf_cfg_file = prog_dir / ::umb::g_default_clang_format_config;

    boost::asio::io_context ctx;
    for (const auto& path: paths)
    {
        const auto style_arg = std::format("--style=file:{}", cf_cfg_file.string());
        const auto& pstr = path.string();
        std::cout << std::format("running clang-format on '{}'\n", pstr);
        bp::execute(bp::process(ctx, cf_prog, {pstr, style_arg, "-i", "--Werror"}));
    }
    ctx.run();
}

void render_cpp(inja::Environment& env,
                const std::string& hdr_template_file,
                const std::string& src_template_file,
                inja::json& data,
                const std::string& cpp_out_dir,
                bool run_clang_format = true)
{
    if (!data.contains("cpp_namespace"))
    {
        auto class_name = data["class_name"].get<std::string>();
        boost::to_lower(class_name);
        const auto cn = std::format("{}::umb", class_name);
        data["cpp_namespace"] = cn;
    }

    const auto hdr = env.render_file(hdr_template_file, data);
    const auto src = env.render_file(src_template_file, data);
    fs::path out_filename = fs::path(data["class_name"].get<std::string>()).filename();
    fs::path hdr_out_file =
        fs::path{cpp_out_dir} / out_filename.replace_extension(::umb::g_cpp_hdr_extension);
    // Reset extension to avoid double extension (umb.umb.cpp).
    out_filename.replace_extension("");
    out_filename.replace_extension("");
    fs::path src_out_file =
        fs::path{cpp_out_dir} / out_filename.replace_extension(::umb::g_cpp_src_extension);
    std::cout << std::format("writing: '{}'\n", hdr_out_file.string());
    std::cout << std::format("writing: '{}'\n", src_out_file.string());
    fs::ofstream hdr_out{hdr_out_file};
    fs::ofstream src_out{src_out_file};
    hdr_out << hdr;
    src_out << src;

    if (run_clang_format)
    {
        clang_format({hdr_out_file, src_out_file});
    }
}

void
render(
    const std::string& file,
    const std::string& uscript_out_dir,
    const std::string& cpp_out_dir,
    const std::string& uscript_test_mutator = "",
    bool run_clang_format = true)
{
    inja::Environment env;

    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);
    env.set_throw_at_missing_includes(true);

    // TODO: add option to enable/disable this?
    env.add_callback("capitalize", 1, capitalize);
    env.add_callback("pad", 2, pad);
    env.add_callback("var_bool", var_bool);
    env.add_callback("var_int", var_int);
    env.add_callback("cpp_type", 1, cpp_type);
    env.add_callback("cpp_type_arg", 1, cpp_type_arg);
    env.add_callback("cpp_default_value", 1, cpp_default_value);
    env.add_callback("uscript_type", 1, uscript_type);
    env.add_callback("bp_is_packed", 2, bp_is_packed);
    env.add_callback("bp_pack_index", 2, bp_pack_index);
    env.add_callback("bp_is_last", 2, bp_is_last);
    env.add_callback("bp_is_multi_pack_boundary", 2, bp_is_multi_pack_boundary);
    env.add_callback("meta_field_type", 1, meta_field_type);
    env.add_void_callback("error", error);

    auto data = env.load_json(file);
    auto class_name = fs::path(file).stem().string();
    class_name.at(0) = static_cast<char>(std::toupper(class_name.at(0)));
    data["class_name"] = class_name;

    auto& messages = data["messages"];

    for (auto& message: messages)
    {
        auto result = analyze_message(message);
        std::cout << result << "\n";
        message["has_static_size"] = result.has_static_size;
        message["always_single_part"] = result.always_single_part;
        message["static_size"] = result.static_size;
        message["static_part"] = result.static_part;
        message["has_float_fields"] = result.has_float_fields;
        message["has_string_fields"] = result.has_string_fields;
        message["has_bytes_fields"] = result.has_bytes_fields;

        message["bool_packs"] = std::vector<inja::json>{};
        for (const auto& bp: result.bool_packs)
        {
            auto bp_json = inja::json{};
            bp_json["field_name"] = bp.field_name;
            bp_json["field_index"] = bp.field_index;
            bp_json["pack_index"] = bp.pack_index;
            bp_json["byte"] = bp.byte;
            bp_json["last"] = bp.last;
            bp_json["boundary"] = bp.boundary;
            message["bool_packs"].emplace_back(bp_json);
        }
    }

    data["uscript_message_type_prefix"] = "EMT";

    if (!data.contains("uscript_message_handler_class"))
    {
        data["uscript_message_handler_class"] = "MessageHandler";
    }

    data["cpp_hdr_extension"] = ::umb::g_cpp_hdr_extension;
    data["cpp_src_extension"] = ::umb::g_cpp_src_extension;

    data["bools_in_byte"] = ::umb::g_bools_in_byte;
    data["max_message_count"] = ::umb::g_max_message_count;
    data["header_size"] = ::umb::g_header_size;
    data["payload_size"] = ::umb::g_payload_size;
    data["packet_size"] = ::umb::g_packet_size;
    data["sizeof_uscript_char"] = ::umb::g_sizeof_uscript_char;
    data["max_dynamic_size"] = ::umb::g_max_dynamic_size;
    data["generate_meta_cpp"] = g_generate_meta_cpp;

    const auto prog_dir = boost::dll::program_location().parent_path();
    fs::path template_dir = prog_dir / ::umb::g_template_dir;
    fs::path us_template = template_dir / ::umb::g_uscript_template;
    fs::path cpp_hdr_template = template_dir / ::umb::g_cpp_hdr_template;
    fs::path cpp_src_template = template_dir / ::umb::g_cpp_src_template;
    fs::path us_test_mutator_template = template_dir / ::umb::g_uscript_test_mutator_template;

    const auto uscript_out_name = fs::path(
        data["class_name"].get<std::string>()).filename().replace_extension(".uc").string();

    std::cout << std::format("rendering '{}'\n", file);
    render_uscript(env, us_template.string(), data, uscript_out_name, uscript_out_dir);
    render_cpp(env, cpp_hdr_template.string(), cpp_src_template.string(), data, cpp_out_dir,
               run_clang_format);

    const bool should_gen_mutator = data["__generate_test_mutator"].get<bool>();

    if (should_gen_mutator && !uscript_test_mutator.empty())
    {
        data["uscript_test_mutator"] = uscript_test_mutator;
        const auto us_test_cmdlet_out_name = uscript_test_mutator + ".uc";
        render_uscript(env, us_test_mutator_template.string(), data, us_test_cmdlet_out_name,
                       uscript_out_dir);
    }
}

} // namespace

// TODO: split this into a library + executable?
//   -> easier to test generation library in unit tests
int main(int argc, char* argv[])
{
    bool run_clang_format = true;

    try
    {
        const auto default_out_dir = fs::current_path().parent_path();

        po::options_description desc("Options");
        desc.add_options()("help,h", "print the help message");
        desc.add_options()("no-run-clang-format",
                           po::bool_switch(&run_clang_format),
                           "skips clang-format step for generated C++ if set");
        desc.add_options()("uscript-out,u",
                           po::value<std::string>()->default_value(default_out_dir.string()),
                           "UnrealScript code generation output directory");
        desc.add_options()("cpp-out,c",
                           po::value<std::string>()->default_value(default_out_dir.string()),
                           "C++ code generation output directory");
        desc.add_options()("input-file",
                           po::value<std::vector<std::string>>()->required(), "input file(s)");
        desc.add_options()("uscript-test-mutator",
                           po::value<std::string>()->default_value(""),
                           "name of the generated UnrealScript test suite mutator");

        po::positional_options_description p;
        p.add("input-file", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

        if (vm.count("help"))
        {
            std::cout << "Usage: " << argv[0] << " [input-file ...]\n";
            std::cout << desc << std::endl;
            return EXIT_FAILURE;
        }

        po::notify(vm);

        if (vm.count("input-file") == 0)
        {
            std::cout << "no input file(s)" << std::endl;
            return EXIT_FAILURE;
        }

        const auto uscript_out_dir = vm["uscript-out"].as<std::string>();
        const auto cpp_out_dir = vm["cpp-out"].as<std::string>();
        auto uscript_test_mutator = vm["uscript-test-mutator"].as<std::string>();

        for (const auto& file: vm["input-file"].as<std::vector<std::string>>())
        {
            render(file, uscript_out_dir, cpp_out_dir, uscript_test_mutator, run_clang_format);
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << "error: " << ex.what() << std::endl;
#ifndef NDEBUG
        throw;
#else
        return EXIT_FAILURE;
#endif
    }
    catch (...)
    {
        std::cout << "unknown error" << std::endl;
#ifndef NDEBUG
        throw;
#else
        return EXIT_FAILURE;
#endif
    }

    return EXIT_SUCCESS;
}
