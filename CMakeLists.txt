cmake_minimum_required(VERSION 3.25)
project(uscript-msgbuf)

set(CMAKE_CXX_STANDARD 20)

find_package(
    inja
    CONFIG
    REQUIRED
)
find_package(
    nlohmann_json
    CONFIG
    REQUIRED
)
find_package(
    Boost
    COMPONENTS
    program_options
    REQUIRED
)

add_executable(
    uscript_msgbuf_generator
    main.cpp
)

target_link_libraries(
    uscript_msgbuf_generator
    PRIVATE
    pantor::inja
    nlohmann_json::nlohmann_json
    Boost::program_options
)
