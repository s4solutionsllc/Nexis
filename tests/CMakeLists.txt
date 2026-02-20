# Test sources are listed explicitly. When adding a test file, update this list.
add_executable(nexis-tests
  utils/test_format_util.cpp
)

target_link_libraries(nexis-tests
  nexis-core
  Qt6::Test
)

add_test(NAME FormatUtilTests COMMAND nexis-tests)
