#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <inja/inja.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include "umb/constants.hpp"
#include "umb/message.hpp"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

// Inja does not support Jinja macros so let's use these to
// pass variables in and out of include blocks.
static std::unordered_map<std::string, size_t> g_variables{
    {"x", 0},
};

enum class VarAction
{
    GET = 0,
    SET = 1,
};

const static std::unordered_map<std::string, VarAction> g_str_to_varaction{
    {"GET", VarAction::GET},
    {"SET", VarAction::SET},
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
    std::vector<std::string> types;
    types.reserve(fields.size());
    for (const auto& field: fields)
    {
        types.emplace_back(field["type"]);
    }

    result.has_static_size = std::all_of(types.cbegin(), types.cend(), [](const std::string& type)
    {
        return ::umb::g_static_types.contains(type);
    });

    auto static_size = ::umb::g_header_size;
    for (const auto& type: types)
    {
        if (::umb::g_static_types.contains(type))
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

    if (result.has_static_size && result.static_size <= 255)
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

    return result;
}

constexpr auto capitalize = [](inja::Arguments& args)
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
constexpr auto pad = [](inja::Arguments& args)
{
    const auto index = args.at(0)->get<size_t>();
    const auto size = args.at(1)->get<size_t>();
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
static constexpr auto var = [](inja::Arguments& args)
{
    const auto var_name = args.at(0)->get<std::string>();
    auto action = args.at(1)->get<std::string>();
    size_t value;

    boost::algorithm::to_upper(action);
    VarAction va = g_str_to_varaction.at(action);

    switch (va)
    {
        case VarAction::GET:
            return ::g_variables.at(var_name);
        case VarAction::SET:
            value = args.at(2)->get<size_t>();
            ::g_variables.at(var_name) = value;
            return value;
        default:
            throw std::invalid_argument("invalid VarAction: " + action);
    }
};

constexpr auto error = [](inja::Arguments& args)
{
    std::stringstream ss;
    for (const auto& arg: args)
    {
        ss << arg->get<std::string>();
    }
    throw std::invalid_argument(ss.str());
};

constexpr auto cpp_type = [](inja::Arguments& args)
{
    const auto type = args.at(0)->get<std::string>();
    return ::umb::g_type_to_cpp_type.at(type);
};

constexpr auto cpp_type_arg = [](inja::Arguments& args)
{
    const auto type = args.at(0)->get<std::string>();
    return ::umb::g_type_to_cpp_type_arg.at(type);
};

constexpr auto cpp_default_value = [](inja::Arguments& args)
{
    const auto type = args.at(0)->get<std::string>();
    return ::umb::g_cpp_default_value.at(type);
};

void render_uscript(inja::Environment& env, const std::string& file, const inja::json& data,
                    const std::string& uscript_out_dir)
{
    const auto r = env.render_file(file, data);
    std::cout << r << "\n";
    fs::path out_filename = fs::path(
        data["class_name"].get<std::string>()).filename().replace_extension(".uc");
    fs::path out_file = fs::path{uscript_out_dir} / out_filename;
    std::cout << "writing '" << out_file.string() << "'\n";
    fs::ofstream out{out_file};
    out << r;
}

void render_cpp(inja::Environment& env, const std::string& hdr_template_file,
                const std::string& src_template_file, const inja::json& data,
                const std::string& cpp_out_dir)
{
    const auto hdr = env.render_file(hdr_template_file, data);
    const auto src = env.render_file(src_template_file, data);
    std::cout << hdr << "\n";
    std::cout << src << "\n";
    fs::path out_filename = fs::path(data["class_name"].get<std::string>()).filename();
    fs::path hdr_out_file =
        fs::path{cpp_out_dir} / out_filename.replace_extension(::umb::g_cpp_hdr_extension);
    fs::path src_out_file =
        fs::path{cpp_out_dir} / out_filename.replace_extension(::umb::g_cpp_src_extension);;
    std::cout << "writing '" << hdr_out_file.string() << "'\n";
    std::cout << "writing '" << src_out_file.string() << "'\n";
    fs::ofstream hdr_out{hdr_out_file};
    fs::ofstream src_out{src_out_file};
    hdr_out << hdr;
    src_out << src;
}

void
render(const std::string& file, const std::string& uscript_out_dir, const std::string& cpp_out_dir)
{
    inja::Environment env;

    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);
    env.set_throw_at_missing_includes(true);

    // TODO: add option to enable/disable this.
    env.add_callback("capitalize", 1, capitalize);
    env.add_callback("pad", 2, pad);
    env.add_callback("var", var);
    env.add_callback("cpp_type", 1, cpp_type);
    env.add_callback("cpp_type_arg", 1, cpp_type_arg);
    env.add_callback("cpp_default_value", 1, cpp_default_value);
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
    }

    data["uscript_message_type_prefix"] = "EMT";

    if (!data.contains("uscript_message_handler_class"))
    {
        data["uscript_message_handler_class"] = "MessageHandler";
    }

    data["cpp_hdr_extension"] = ::umb::g_cpp_hdr_extension;
    data["cpp_src_extension"] = ::umb::g_cpp_src_extension;

    data["header_size"] = ::umb::g_header_size;
    data["payload_size"] = ::umb::g_payload_size;
    data["packet_size"] = ::umb::g_packet_size;
    data["float_multiplier"] = ::umb::g_float_multiplier;

    fs::path template_dir{::umb::g_template_dir};
    fs::path us_template = template_dir / ::umb::g_uscript_template;
    fs::path cpp_hdr_template = template_dir / ::umb::g_cpp_hdr_template;
    fs::path cpp_src_template = template_dir / ::umb::g_cpp_src_template;

    std::cout << "rendering '" << file << "'\n";
    render_uscript(env, us_template.string(), data, uscript_out_dir);
    render_cpp(env, cpp_hdr_template.string(), cpp_src_template.string(), data, cpp_out_dir);
}

int main(int argc, char* argv[])
{
    try
    {
        const auto default_out_dir = fs::current_path().parent_path();

        po::options_description desc("Options");
        desc.add_options()("help,h", "print the help message");
        desc.add_options()("uscript-out,u",
                           po::value<std::string>()->default_value(default_out_dir.string()),
                           "UnrealScript code generation output directory");
        desc.add_options()("cpp-out,c",
                           po::value<std::string>()->default_value(default_out_dir.string()),
                           "C++ code generation output directory");
        desc.add_options()("input-file",
                           po::value<std::vector<std::string>>()->required(), "input file(s)");

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

        for (const auto& file: vm["input-file"].as<std::vector<std::string>>())
        {
            render(file, uscript_out_dir, cpp_out_dir);
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << "error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout << "unknown error" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
