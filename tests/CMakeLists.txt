find_package(doctest REQUIRED)

add_executable(test_coding test_coding.cpp)
target_link_libraries(test_coding PRIVATE doctest::doctest umb)
add_test(NAME test_coding COMMAND test_coding)
target_compile_options(test_coding PRIVATE ${UMB_COMPILE_OPTIONS})
target_compile_features(test_coding PRIVATE cxx_std_23)

file(GLOB TEST_MSG_FILES data/*.json)
add_custom_command(
    generate_test_data

)
