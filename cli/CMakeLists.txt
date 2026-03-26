find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

set(_karing_saved_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_library(JSONCPP_STATIC_LIBRARY NAMES jsoncpp)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_karing_saved_library_suffixes})

if(JSONCPP_STATIC_LIBRARY)
  add_library(karing_jsoncpp STATIC IMPORTED)
  set_target_properties(karing_jsoncpp PROPERTIES
    IMPORTED_LOCATION "${JSONCPP_STATIC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
  )
else()
  add_library(karing_jsoncpp INTERFACE IMPORTED)
  target_include_directories(karing_jsoncpp INTERFACE ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(karing_jsoncpp INTERFACE ${JSONCPP_LINK_LIBRARIES})
endif()

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
)

target_link_libraries(karing_cli_core
  PRIVATE karing_project_options CURL::libcurl karing_jsoncpp
)

add_executable(karing
  main.cpp
)

target_include_directories(karing
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(karing
  PRIVATE karing_project_options karing_cli_core CURL::libcurl karing_jsoncpp
)

install(TARGETS karing RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
