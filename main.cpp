#include <iostream>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    std::cout << "Hello, World!" << std::endl;

    inja::Environment env;

    return EXIT_SUCCESS;
}
