#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

static std::unordered_map<std::string, size_t> static_types{
    {"byte", 1},
    {"int", 4},
    {"float", 4},
};

struct MsgAnalysisResult
{
    friend std::ostream& operator<<(std::ostream& os, const MsgAnalysisResult& result);

    std::string name{};
    bool has_static_size{false};
    size_t static_size{0};
    bool always_single_part{false};
};

std::ostream& operator<<(std::ostream& os, const MsgAnalysisResult& result)
{
    os << "{" << " name: " << result.name << ", has_static_size: " << result.has_static_size
       << ", static_size: " << result.static_size << ", always_single_part: "
       << result.always_single_part << " }";
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
        return static_types.contains(type);
    });

    if (result.has_static_size)
    {
        for (const auto& type: types)
        {
            result.static_size += static_types.at(type);
        }
    }

    if (result.has_static_size && result.static_size <= 255)
    {
        result.always_single_part = true;
    }

    return result;
}

void render_uscript(inja::Environment& env, const std::string& file, const inja::json& data)
{
    std::cout << env.render_file(file, data) << "\n";
}

void render(const std::string& file)
{
    inja::Environment env;

    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);

    // TODO: add option to enable/disable this.
    env.add_callback("capitalize", 1, [](inja::Arguments& args)
    {
        auto str = args.at(0)->get<std::string>();
        str[0] = static_cast<char>(std::toupper(str[0]));
        return str;
    });

    auto data = env.load_json(
        R"(C:\Users\tkrii\Documents\My Games\Rising Storm 2\ROGame\Src\VOIPTest\Messages.json)");
    data["class_name"] = file;

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

    std::cout << "rendering '" << file << "'\n";
    render_uscript(env, file, data);
}

int main(int argc, char* argv[])
{
    try
    {
        po::options_description desc("Options");
        desc.add_options()("help,h", "print the help message");
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

        for (const auto& file: vm["input-file"].as<std::vector<std::string>>())
        {
            render(file);
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
    }

    return EXIT_SUCCESS;
}
