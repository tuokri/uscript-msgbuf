cmake_minimum_required(VERSION 3.25)
project(uscript-msgbuf)

include(CMakePrintHelpers)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(Boost_USE_DEBUG_LIBS $<$<CONFIG:Debug>:ON,OFF>)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)

find_package(
    inja
    CONFIG
    REQUIRED
)

find_package(
    Boost
    COMPONENTS
    system
    filesystem
    program_options
    REQUIRED
)

add_library(umb INTERFACE)

target_include_directories(umb INTERFACE include)

add_executable(
    uscript_msgbuf_generator
    src/main.cpp
)

if (MSVC)
    set(
        UMB_GENERATOR_LINK_LIBS
        umb
        pantor::inja
        Boost::boost
        Boost::system
        Boost::filesystem
        Boost::program_options
    )
else ()
    set(
        UMB_GENERATOR_LINK_LIBS
        umb
        pantor::inja
        Boost::boost
        Boost::system
        Boost::filesystem
        Boost::program_options
        dl
    )
endif ()

target_link_libraries(
    uscript_msgbuf_generator
    PRIVATE
    ${UMB_GENERATOR_LINK_LIBS}
)

target_compile_features(
    uscript_msgbuf_generator
    PRIVATE
    cxx_std_23
)

target_compile_features(
    umb
    INTERFACE
    cxx_std_23
)

# Build list of file paths to copy templates to.
file(GLOB TEMPLATE_FILES templates/*.jinja)
set(TEMPLATE_FILES ${TEMPLATE_FILES})
foreach (template_file ${TEMPLATE_FILES})
    cmake_path(GET template_file FILENAME template_filename)
    cmake_path(
        SET
        template_file_output
        NORMALIZE
        ${CMAKE_CURRENT_BINARY_DIR}/templates/${template_filename}
    )
    list(APPEND TEMPLATE_FILE_OUTPUTS ${template_file_output})
endforeach ()

add_custom_target(copy_templates ALL DEPENDS ${TEMPLATE_FILE_OUTPUTS})
add_custom_command(
    OUTPUT ${TEMPLATE_FILE_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/templates/
    COMMAND ${CMAKE_COMMAND} -E copy ${TEMPLATE_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/templates/
    DEPENDS ${TEMPLATE_FILES}
)

add_dependencies(uscript_msgbuf_generator copy_templates)

cmake_path(SET UMB_CLANG_FORMAT_CONFIG_FILE NORMALIZE ${CMAKE_SOURCE_DIR}/.umb.output.clang-format)
cmake_path(SET UMB_CLANG_FORMAT_CONFIG_FILE_COPY_DEST NORMALIZE ${CMAKE_CURRENT_BINARY_DIR}/.umb.output.clang-format)
cmake_print_variables(UMB_CLANG_FORMAT_CONFIG_FILE)
cmake_print_variables(UMB_CLANG_FORMAT_CONFIG_FILE_COPY_DEST)
add_custom_target(copy_clang_format_config ALL DEPENDS ${UMB_CLANG_FORMAT_CONFIG_FILE_COPY_DEST})
add_custom_command(
    OUTPUT ${UMB_CLANG_FORMAT_CONFIG_FILE_COPY_DEST}
    COMMAND ${CMAKE_COMMAND} -E copy ${UMB_CLANG_FORMAT_CONFIG_FILE}
    ${UMB_CLANG_FORMAT_CONFIG_FILE_COPY_DEST}
    DEPENDS ${UMB_CLANG_FORMAT_CONFIG_FILE}
)

add_dependencies(copy_templates copy_clang_format_config)
add_dependencies(uscript_msgbuf_generator copy_clang_format_config)

if (MSVC)
    set(
        UMB_COMPILE_OPTIONS
        /W4
    )
else ()
    set(
        UMB_COMPILE_OPTIONS
        -Wall
        -pedantic
        -Wpedantic
        -Wextra
    )
endif ()
cmake_print_variables(UMB_COMPILE_OPTIONS)

target_compile_options(
    uscript_msgbuf_generator
    PRIVATE
    ${UMB_COMPILE_OPTIONS}
)

target_compile_options(
    umb
    INTERFACE
    ${UMB_COMPILE_OPTIONS}
)

option(BUILD_TESTS "build tests" OFF)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
