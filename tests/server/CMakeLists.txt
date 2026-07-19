add_executable(karing_server_tests
  test_server.cpp
  $<TARGET_OBJECTS:karing_server>
)

target_link_libraries(karing_server_tests
  PRIVATE karing_project_options karing_server_support karing_sqlite karing_sqlite_file Drogon::Drogon
)

add_test(NAME karing_server_tests COMMAND karing_server_tests)
