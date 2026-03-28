find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

set(JSONCPP_STATIC_LIBRARY "")

if(JSONCPP_LIBRARY_DIRS)
  foreach(_jsoncpp_dir IN LISTS JSONCPP_LIBRARY_DIRS)
    if(EXISTS "${_jsoncpp_dir}/libjsoncpp.a")
      set(JSONCPP_STATIC_LIBRARY "${_jsoncpp_dir}/libjsoncpp.a")
      break()
    endif()
  endforeach()
endif()

if(NOT JSONCPP_STATIC_LIBRARY)
  execute_process(
    COMMAND pkg-config --variable=libdir jsoncpp
    OUTPUT_VARIABLE _jsoncpp_pkg_libdir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_jsoncpp_pkg_libdir AND EXISTS "${_jsoncpp_pkg_libdir}/libjsoncpp.a")
    set(JSONCPP_STATIC_LIBRARY "${_jsoncpp_pkg_libdir}/libjsoncpp.a")
  endif()
endif()

if(NOT JSONCPP_STATIC_LIBRARY)
  set(_karing_saved_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  find_library(JSONCPP_STATIC_LIBRARY NAMES jsoncpp)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_karing_saved_library_suffixes})
endif()

if(JSONCPP_STATIC_LIBRARY)
  message(STATUS "CLI: using static jsoncpp: ${JSONCPP_STATIC_LIBRARY}")
  add_library(karing_jsoncpp STATIC IMPORTED)
  set_target_properties(karing_jsoncpp PROPERTIES
    IMPORTED_LOCATION "${JSONCPP_STATIC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
  )
else()
  message(STATUS "CLI: static jsoncpp not found, falling back to shared/system link")
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
  commands/swap_command.cpp
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
