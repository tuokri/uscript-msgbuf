find_package(doctest REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc io)

add_executable(test_coding test_coding.cpp)
target_link_libraries(test_coding PRIVATE doctest::doctest umb test_msg_library)
add_test(NAME test_coding COMMAND test_coding)
target_compile_options(test_coding PRIVATE ${UMB_COMPILE_OPTIONS})
target_compile_features(test_coding PRIVATE cxx_std_23)
add_dependencies(test_coding generate_test_data copy_templates)

set(UMB_GENERATED_TEST_OUT ${CMAKE_CURRENT_SOURCE_DIR}/out)
set(UMB_GENERATED_OUTPUT_TEST_FILES "")
file(GLOB TEST_MSG_FILES data/*.json)
# TODO: have the generator output this list instead of building it manually here?
foreach (test_msg_file ${TEST_MSG_FILES})
    cmake_path(GET test_msg_file STEM test_msg_stem)
    cmake_path(SET msg_out_uc NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.uc)
    cmake_path(SET msg_out_cpp NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.umb.cpp)
    cmake_path(SET msg_out_hpp NORMALIZE ${UMB_GENERATED_TEST_OUT}/${test_msg_stem}.umb.hpp)
    list(APPEND UMB_GENERATED_OUTPUT_TEST_FILES ${msg_out_uc} ${msg_out_cpp} ${msg_out_hpp})
endforeach ()
add_custom_command(
    OUTPUT ${UMB_GENERATED_OUTPUT_TEST_FILES}
    COMMAND ${CMAKE_BINARY_DIR}/uscript_msgbuf_generator${CMAKE_EXECUTABLE_SUFFIX}
    ${TEST_MSG_FILES}
    --cpp-out=${UMB_GENERATED_TEST_OUT}
    --uscript-out=${UMB_GENERATED_TEST_OUT}
    DEPENDS ${TEST_MSG_FILES} ${TEMPLATE_FILES}
)
add_custom_target(generate_test_data ALL DEPENDS ${UMB_GENERATED_OUTPUT_TEST_FILES})
add_dependencies(generate_test_data uscript_msgbuf_generator)
add_dependencies(generate_test_data copy_templates)

add_library(test_msg_library ${UMB_GENERATED_OUTPUT_TEST_FILES})
target_include_directories(test_msg_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/out)
target_link_libraries(test_msg_library PUBLIC umb ICU::uc ICU::io)
add_dependencies(
    test_msg_library
    uscript_msgbuf_generator
    copy_templates generate_test_data
)

# TODO: clean up all the unneeded dependency links. The graph is a mess.
