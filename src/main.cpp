#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <inja/inja.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

namespace fs = boost::filesystem;
namespace po = boost::program_options;

static constexpr auto g_template_dir = "templates";
static constexpr auto g_cpp_template = "cpp.jinja";
static constexpr auto g_uscript_template = "uscript.jinja";
static constexpr auto g_uscript_message_handler_template = "uscript_message_handler.jinja";

// Packet header size in bytes.
static constexpr size_t g_header_size = 3;

// All types with known static sizes in bytes.
static const std::unordered_map<std::string, size_t> g_static_types{
    {"byte",  1},
    {"int",   4},
    {"float", 4},
};

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
    size_t static_size{0};
    // Message has static size and is always guaranteed to fit in a single packet.
    bool always_single_part{false};
};

std::ostream& operator<<(std::ostream& os, const MsgAnalysisResult& result)
{
    os << "{"
       << " name: " << result.name
       << ", has_static_size: " << result.has_static_size
       << ", static_size: " << result.static_size
       << ", always_single_part: " << result.always_single_part
       << " }";
    return os;
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
        return ::g_static_types.contains(type);
    });

    if (result.has_static_size)
    {
        result.static_size = ::g_header_size;
        for (const auto& type: types)
        {
            result.static_size += ::g_static_types.at(type);
        }
    }

    if (result.has_static_size && result.static_size <= 255)
    {
        result.always_single_part = true;
    }

    return result;
}

static constexpr auto capitalize = [](inja::Arguments& args)
{
    auto str = args.at(0)->get<std::string>();
    str[0] = static_cast<char>(std::toupper(str[0]));
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
static constexpr auto pad = [](inja::Arguments& args)
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
    for (int i = 0; i < pad_count; ++i)
    {
        ss << " ";
    }
    ret = ss.str();
    return ret;
};

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

void render_uscript(inja::Environment& env, const std::string& file, const inja::json& data,
                    const std::string& uscript_out_dir)
{
    const auto r = env.render_file(file, data);
    std::cout << r << "\n";
    fs::path out_filename = fs::path(file).replace_filename(
        data["class_name"].get<std::string>()).filename().replace_extension(".uc");
    fs::path out_file = fs::path{uscript_out_dir} / out_filename;
    std::cout << "writing '" << out_file.string() << "'\n";
    fs::ofstream out{out_file};
    out << r << std::endl;
}

void render_cpp(inja::Environment& env, const std::string& file, const inja::json& data,
                const std::string& cpp_out_dir)
{
}

void
render(const std::string& file, const std::string& uscript_out_dir, const std::string& cpp_out_dir)
{
    inja::Environment env;

    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);

    // TODO: add option to enable/disable this.
    env.add_callback("capitalize", 1, capitalize);
    env.add_callback("pad", 2, pad);
    env.add_callback("var", var);

    auto data = env.load_json(file);
    auto class_name = fs::path(file).stem().string();
    class_name[0] = static_cast<char>(std::toupper(class_name[0]));
    data["class_name"] = class_name;

    auto& messages = data["messages"];

    for (auto& message: messages)
    {
        auto result = analyze_message(message);
        std::cout << result << "\n";
        message["has_static_size"] = result.has_static_size;
        message["always_single_part"] = result.always_single_part;
        message["static_size"] = result.static_size;
    }

    data["uscript_message_type_prefix"] = "EMT";

    if (!data.contains("uscript_message_handler_class"))
    {
        data["uscript_message_handler_class"] = "MessageHandler";
    }

    fs::path template_dir{::g_template_dir};
    fs::path us_template = template_dir / ::g_uscript_template;
    fs::path cpp_template = template_dir / ::g_cpp_template;

    std::cout << "rendering '" << file << "'\n";
    render_uscript(env, us_template.string(), data, uscript_out_dir);
    render_cpp(env, cpp_template.string(), data, cpp_out_dir);
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
