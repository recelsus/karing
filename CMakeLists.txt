cmake_minimum_required(VERSION 3.22)
project(karing LANGUAGES CXX)

# Export compile_commands.json for LSP/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(karing
  src/main.cpp
  src/db/db_init.cpp
  src/db/schema_pragmas.cpp
  src/db/schema_config.cpp
  src/db/schema_karing.cpp
  src/db/schema_fts.cpp
  src/db/schema_auth.cpp
  src/db/schema_overwrite.cpp
  src/db/db_introspection.cpp
  src/utils/embedded_config.cpp
  src/dao/karing_dao.cpp
  src/controllers/karing_controller.cpp
  src/controllers/admin_controller.cpp
  src/controllers/root_controller.cpp
  src/controllers/health_controller.cpp
  src/utils/options.cpp
  src/utils/json_response.cpp
  src/utils/search_query.cpp
  src/utils/cursor.cpp
  src/utils/strings.cpp
  src/filters/auth_filter.cpp
  src/filters/admin_filter.cpp
  src/filters/https_redirect_filter.cpp
  src/filters/hsts_filter.cpp
)

find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(karing PRIVATE Drogon::Drogon)
target_link_libraries(karing PRIVATE sqlite3)
target_link_libraries(karing PRIVATE OpenSSL::Crypto)
set(KARING_BUILD_LIMIT "100" CACHE STRING "Runtime upper bound (<=100)")
# Clamp invalid values at configure time
if (KARING_BUILD_LIMIT GREATER 100)
  message(WARNING "KARING_BUILD_LIMIT > 100; clamping to 100")
  set(KARING_BUILD_LIMIT 100 CACHE STRING "clamped" FORCE)
endif()
if (KARING_BUILD_LIMIT LESS 1)
  set(KARING_BUILD_LIMIT 1 CACHE STRING "clamped" FORCE)
endif()
target_include_directories(karing PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
add_compile_definitions(KARING_MAX_LIMIT=100)
add_compile_definitions(KARING_BUILD_LIMIT=${KARING_BUILD_LIMIT})
add_compile_definitions(KARING_CXX_STANDARD=${CMAKE_CXX_STANDARD})
add_compile_definitions(KARING_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\")

# Build-time embedded config (optional)
set(KARING_EMBED_CONFIG_FILE "" CACHE FILEPATH "Path to JSON to embed as default config")
set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

set(_HAVE_JSON 0)
set(_EMBED_JSON_ESCAPED "")
if (KARING_EMBED_CONFIG_FILE)
  if (EXISTS "${KARING_EMBED_CONFIG_FILE}")
    file(READ "${KARING_EMBED_CONFIG_FILE}" _RAW_JSON)
    string(REPLACE "\\" "\\\\" _EMBED_JSON_ESCAPED "${_RAW_JSON}")
    string(REPLACE "\"" "\\\"" _EMBED_JSON_ESCAPED "${_EMBED_JSON_ESCAPED}")
    string(REPLACE "\n" "\\n" _EMBED_JSON_ESCAPED "${_EMBED_JSON_ESCAPED}")
    set(_HAVE_JSON 1)
  else()
    message(WARNING "KARING_EMBED_CONFIG_FILE path does not exist: ${KARING_EMBED_CONFIG_FILE}")
  endif()
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/embedded_user_json.h.in
  ${GENERATED_INCLUDE_DIR}/embedded_user_json.h
  @ONLY
)
target_include_directories(karing PRIVATE ${GENERATED_INCLUDE_DIR})

# Embed git revision and build time
execute_process(
  COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --always --dirty
  OUTPUT_VARIABLE KARING_GIT_DESCRIBE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT KARING_GIT_DESCRIBE)
  execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short=12 HEAD
    OUTPUT_VARIABLE KARING_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT KARING_GIT_DESCRIBE)
  set(KARING_GIT_DESCRIBE "unknown")
endif()
string(TIMESTAMP KARING_BUILD_TIME_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
add_compile_definitions(KARING_GIT_REV=\"${KARING_GIT_DESCRIBE}\")
add_compile_definitions(KARING_BUILD_TIME=\"${KARING_BUILD_TIME_UTC}\")

# Build environment metadata (robust without git)
if(NOT DEFINED KARING_BUILD_HOST)
  set(KARING_BUILD_HOST "$ENV{HOSTNAME}")
  if(NOT KARING_BUILD_HOST)
    execute_process(COMMAND hostname OUTPUT_VARIABLE KARING_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
  if(NOT KARING_BUILD_HOST)
    set(KARING_BUILD_HOST "unknown-host")
  endif()
endif()
if(NOT DEFINED KARING_BUILD_USER)
  set(KARING_BUILD_USER "$ENV{USER}")
  if(NOT KARING_BUILD_USER)
    set(KARING_BUILD_USER "$ENV{USERNAME}")
  endif()
  if(NOT KARING_BUILD_USER)
    set(KARING_BUILD_USER "unknown-user")
  endif()
endif()
set(KARING_BUILD_OS "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(KARING_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
set(KARING_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
set(KARING_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
set(KARING_CMAKE_VERSION "${CMAKE_VERSION}")

add_compile_definitions(KARING_BUILD_HOST=\"${KARING_BUILD_HOST}\")
add_compile_definitions(KARING_BUILD_USER=\"${KARING_BUILD_USER}\")
add_compile_definitions(KARING_BUILD_OS=\"${KARING_BUILD_OS}\")
add_compile_definitions(KARING_COMPILER_ID=\"${KARING_COMPILER_ID}\")
add_compile_definitions(KARING_COMPILER_VERSION=\"${KARING_COMPILER_VERSION}\")
add_compile_definitions(KARING_CMAKE_GENERATOR=\"${KARING_CMAKE_GENERATOR}\")
add_compile_definitions(KARING_CMAKE_VERSION=\"${KARING_CMAKE_VERSION}\")

# Build number and git branch
if(NOT DEFINED KARING_BUILD_NUMBER)
  execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-list --count HEAD
    OUTPUT_VARIABLE KARING_BUILD_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT KARING_BUILD_NUMBER)
  set(KARING_BUILD_NUMBER "0")
endif()
if(NOT DEFINED KARING_GIT_BRANCH)
  execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE KARING_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT KARING_GIT_BRANCH)
  set(KARING_GIT_BRANCH "unknown-branch")
endif()
add_compile_definitions(KARING_BUILD_NUMBER=\"${KARING_BUILD_NUMBER}\")
add_compile_definitions(KARING_GIT_BRANCH=\"${KARING_GIT_BRANCH}\")

# Create/refresh a top-level compile_commands.json symlink (or copy on Windows)
if (EXISTS ${CMAKE_BINARY_DIR}/compile_commands.json)
  if (WIN32)
    add_custom_target(link_compile_commands ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${CMAKE_BINARY_DIR}/compile_commands.json
              ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMENT "Copy compile_commands.json to project root for clangd")
  else()
    add_custom_target(link_compile_commands ALL
      COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMAND ${CMAKE_COMMAND} -E create_symlink
              ${CMAKE_BINARY_DIR}/compile_commands.json
              ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMENT "Symlink compile_commands.json to project root for clangd")
  endif()
endif()

# Install rules (make install)
include(GNUInstallDirs)
install(TARGETS karing RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/karing.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/karing)

# Tests (Catch2 + CTest)
include(CTest)
if (BUILD_TESTING)
  include(FetchContent)
  set(CATCH_VERSION v3.5.3)
  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG ${CATCH_VERSION}
  )
  FetchContent_MakeAvailable(catch2)

  add_executable(karing_tests
    tests/strings_test.cpp
    tests/cursor_test.cpp
    tests/search_query_test.cpp
    src/utils/strings.cpp
    src/utils/cursor.cpp
    src/utils/search_query.cpp
  )
  target_include_directories(karing_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(karing_tests PRIVATE Catch2::Catch2WithMain OpenSSL::Crypto)

  add_test(NAME karing_tests COMMAND karing_tests)
endif()
