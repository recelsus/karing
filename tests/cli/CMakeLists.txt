add_executable(karing_cli_tests
  test_cli.cpp
)

target_include_directories(karing_cli_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/cli
)

target_link_libraries(karing_cli_tests
  PRIVATE karing_project_options karing_cli_core
)

add_test(NAME karing_cli_tests COMMAND karing_cli_tests)
