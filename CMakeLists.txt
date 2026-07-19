cmake_minimum_required(VERSION 3.22)
project(karing VERSION 0.2.0 LANGUAGES CXX)

# Export compile_commands.json for LSP/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KARING_BUILD_SERVER "Build karing-server" ON)
option(KARING_BUILD_CLI "Build karing CLI" ON)
option(KARING_CLI_ENABLE_HTTP_BACKEND "Build karing CLI with HTTP backend support" ON)

if(NOT KARING_BUILD_SERVER AND NOT KARING_BUILD_CLI)
  message(FATAL_ERROR "At least one of KARING_BUILD_SERVER or KARING_BUILD_CLI must be ON")
endif()

if(KARING_BUILD_SERVER)
  find_package(Drogon REQUIRED)
endif()

set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
if(KARING_BUILD_SERVER OR KARING_BUILD_CLI)
  file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sql/schema_base.sql" KARING_SCHEMA_BASE_SQL_RAW)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sql/schema_fts.sql" KARING_SCHEMA_FTS_SQL_RAW)
  string(REPLACE "\\" "\\\\" KARING_SCHEMA_BASE_SQL_ESCAPED "${KARING_SCHEMA_BASE_SQL_RAW}")
  string(REPLACE "\"" "\\\"" KARING_SCHEMA_BASE_SQL_ESCAPED "${KARING_SCHEMA_BASE_SQL_ESCAPED}")
  string(REPLACE "\n" "\\n" KARING_SCHEMA_BASE_SQL_ESCAPED "${KARING_SCHEMA_BASE_SQL_ESCAPED}")
  string(REPLACE "\\" "\\\\" KARING_SCHEMA_FTS_SQL_ESCAPED "${KARING_SCHEMA_FTS_SQL_RAW}")
  string(REPLACE "\"" "\\\"" KARING_SCHEMA_FTS_SQL_ESCAPED "${KARING_SCHEMA_FTS_SQL_ESCAPED}")
  string(REPLACE "\n" "\\n" KARING_SCHEMA_FTS_SQL_ESCAPED "${KARING_SCHEMA_FTS_SQL_ESCAPED}")

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sql/schema_sql.h.in
    ${GENERATED_INCLUDE_DIR}/schema_sql.h
    @ONLY
  )
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/karing_version.h.in
    ${GENERATED_INCLUDE_DIR}/karing_version.h
    @ONLY
  )
endif()

add_library(karing_project_options INTERFACE)
target_include_directories(karing_project_options INTERFACE
  ${CMAKE_SOURCE_DIR}
  ${GENERATED_INCLUDE_DIR}
)

add_library(karing_common STATIC
  common/error/app_error.cpp
)

target_link_libraries(karing_common
  PUBLIC karing_project_options
)

add_library(karing_file_storage STATIC
  sqlite/storage/file_storage.cpp
)

target_link_libraries(karing_file_storage
  PUBLIC karing_project_options
)

target_include_directories(karing_file_storage
  PUBLIC
    ${CMAKE_SOURCE_DIR}/sqlite
)

include(GNUInstallDirs)

if(KARING_BUILD_SERVER OR KARING_BUILD_CLI)
  add_subdirectory(sqlite)
endif()

if(KARING_BUILD_SERVER)
  add_subdirectory(server)
endif()

if(KARING_BUILD_CLI)
  add_subdirectory(cli)
endif()

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
