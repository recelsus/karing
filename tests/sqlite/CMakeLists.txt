add_executable(karing_sqlite_tests
  test_sqlite.cpp
)

target_link_libraries(karing_sqlite_tests
  PRIVATE karing_project_options karing_sqlite karing_sqlite_file
)

add_test(NAME karing_sqlite_tests COMMAND karing_sqlite_tests)
