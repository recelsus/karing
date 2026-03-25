find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_library(karing_cli_core STATIC
  http/http_client.cpp
  utils/auth_cache.cpp
  utils/cli_output.cpp
  utils/io.cpp
  utils/mime.cpp
  utils/url.cpp
  commands/add_command.cpp
  commands/delete_command.cpp
  commands/find_command.cpp
  commands/get_command.cpp
  commands/health_command.cpp
  commands/mod_command.cpp
  app.cpp
)

target_include_directories(karing_cli_core
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(karing_cli_core
  PRIVATE karing_project_options CURL::libcurl ${JSONCPP_LIBRARIES}
)

add_executable(karing
  main.cpp
)

target_include_directories(karing
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(karing
  PRIVATE karing_project_options karing_cli_core CURL::libcurl ${JSONCPP_LIBRARIES}
)

install(TARGETS karing RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
