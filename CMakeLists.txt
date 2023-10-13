cmake_minimum_required(VERSION 3.25)
project(uscript-msgbuf)

set(CMAKE_CXX_STANDARD 20)

find_package(
    inja
    CONFIG
    REQUIRED
)
find_package(
    Boost
    COMPONENTS
    filesystem
    program_options
    REQUIRED
)

add_executable(
    uscript_msgbuf_generator
    src/main.cpp
)

target_link_libraries(
    uscript_msgbuf_generator
    PRIVATE
    pantor::inja
    Boost::filesystem
    Boost::program_options
)

# Build list of file paths to copy templates to.
file(GLOB TEMPLATE_FILES templates/*.jinja)
set(TEMPLATE_FILE_OUTPUTS "")
foreach (template_file in ${TEMPLATE_FILES})
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
    COMMAND ${CMAKE_COMMAND} -E copy ${TEMPLATE_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/templates/
    DEPENDS ${TEMPLATE_FILES}
)

add_dependencies(uscript_msgbuf_generator copy_templates)
