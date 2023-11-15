# Copyright (C) 2023  Tuomo Kriikkula
# This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
#     along with this program.  If not, see <https://www.gnu.org/licenses/>.

include(CTest)

find_package(doctest REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc dt in io)
find_package(Boost REQUIRED)

add_executable(test_coding test_coding.cpp)
target_link_libraries(test_coding PRIVATE doctest::doctest umb test_msg_library)
add_test(NAME test_coding COMMAND test_coding)
target_compile_options(test_coding PRIVATE ${UMB_COMPILE_OPTIONS})
target_compile_features(test_coding PRIVATE cxx_std_23)

add_executable(test_randomized test_randomized.cpp)
target_link_libraries(test_randomized PRIVATE doctest::doctest umb test_msg_library Boost::boost)
add_test(NAME test_randomized COMMAND test_randomized)
target_compile_options(test_randomized PRIVATE ${UMB_COMPILE_OPTIONS})
target_compile_features(test_randomized PRIVATE cxx_std_23)

# TODO: may need to do this for MSVC/Clang later.
# Currently only GCC works with UMB meta/reflection code.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_randomized PRIVATE -ftemplate-depth=2048)
endif ()

set(UMB_USCRIPT_TEST_MUTATOR "UMBTestsMutator")
set(UMB_GENERATED_TEST_OUT ${CMAKE_CURRENT_SOURCE_DIR}/out)
set(UMB_GENERATED_OUTPUT_TEST_FILES "")
file(GLOB UMB_TEST_MSG_DEFINITION_FILES data/*.json)
cmake_print_variables(UMB_TEST_MSG_DEFINITION_FILES)
# TODO: have the generator output this list instead of building it manually here?
# TODO: this will skip some UScript generated files.
foreach (test_msg_file ${UMB_TEST_MSG_DEFINITION_FILES})
    cmake_path(GET test_msg_file STEM test_msg_stem)
    cmake_path(SET msg_out_uc NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.uc)
    cmake_path(SET msg_out_cpp NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.umb.cpp)
    cmake_path(SET msg_out_hpp NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.umb.hpp)
    list(APPEND UMB_GENERATED_OUTPUT_TEST_FILES ${msg_out_uc} ${msg_out_cpp} ${msg_out_hpp})
endforeach ()
list(APPEND UMB_GENERATED_OUTPUT_TEST_FILES ${UMB_GENERATED_TEST_OUT}/${UMB_USCRIPT_TEST_MUTATOR}.uc)
cmake_print_variables(UMB_GENERATED_OUTPUT_TEST_FILES)
if (NOT DEFINED UMB_TEMPLATE_FILES)
    message(FATAL_ERROR "UMB_TEMPLATE_FILES not defined")
endif ()
set(
    UMB_GENERATOR_TESTING_ARGS
    ${UMB_TEST_MSG_DEFINITION_FILES}
    --cpp-out=${UMB_GENERATED_TEST_OUT}
    --uscript-out=${UMB_GENERATED_TEST_OUT}  # TODO: put generated code in build dir!
    --uscript-test-mutator=${UMB_USCRIPT_TEST_MUTATOR}
)
if (NOT UMB_RUN_CLANG_FORMAT)
    list(APPEND UMB_GENERATOR_TESTING_ARGS --no-run-clang-format)
endif ()
cmake_print_variables(UMB_GENERATOR_TESTING_ARGS)
add_custom_command(
    OUTPUT ${UMB_GENERATED_OUTPUT_TEST_FILES}
    COMMAND ${CMAKE_BINARY_DIR}/uscript_msgbuf_generator${CMAKE_EXECUTABLE_SUFFIX}
    ${UMB_GENERATOR_TESTING_ARGS}
    DEPENDS ${UMB_TEST_MSG_DEFINITION_FILES} ${UMB_TEMPLATE_FILES}
)
add_custom_target(generate_test_data ALL DEPENDS ${UMB_GENERATED_OUTPUT_TEST_FILES})
add_dependencies(generate_test_data uscript_msgbuf_generator)
add_dependencies(generate_test_data copy_templates)

# TODO: do not pass .uc files to this library.
add_library(test_msg_library ${UMB_GENERATED_OUTPUT_TEST_FILES})
# Always enable reflection for testing, regardless of global setting.
target_compile_definitions(test_msg_library PRIVATE -DUMB_INCLUDE_META)
target_include_directories(test_msg_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/out)
target_link_libraries(test_msg_library PUBLIC umb ICU::uc ICU::dt ICU::in ICU::io)
target_compile_features(test_msg_library PRIVATE cxx_std_23)

add_dependencies(
    test_msg_library
    uscript_msgbuf_generator
    copy_templates
    generate_test_data
)

add_dependencies(test_coding generate_test_data copy_templates)
add_dependencies(test_randomized generate_test_data copy_templates)

set_property(
    TARGET test_msg_library
    PROPERTY INTERPROCEDURAL_OPTIMIZATION ${UMB_IPO_SUPPORTED}
)

add_executable(
    umb_echo_server
    umb_echo_server.cpp
)
target_link_libraries(
    umb_echo_server
    PRIVATE
    Boost::boost
    umb
    test_msg_library
)

set_property(
    TARGET umb_echo_server
    PROPERTY INTERPROCEDURAL_OPTIMIZATION ${UMB_IPO_SUPPORTED}
)

if (MSCV)
    set(
        UMB_ECHO_SERVER_COMPILE_OPTIONS
        /await
    )
else ()
    set(
        UMB_ECHO_SERVER_COMPILE_OPTIONS
        -fcoroutines
    )
endif ()

target_compile_options(
    umb_echo_server
    PRIVATE
    ${UMB_ECHO_SERVER_COMPILE_OPTIONS}
)

target_compile_features(
    umb_echo_server
    PRIVATE
    cxx_std_23
)

# Avoid executable not being found in later targets due to
# MSVC added /Debug /Release paths.
if (MSVC)
    set_target_properties(umb_echo_server
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
    )
endif ()

# TODO: clean up all the unneeded dependency links. The graph is a mess.
# TODO: make reusable CMake functions to be used in other projects.
