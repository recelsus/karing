add_library(karing_sqlite STATIC
  db/sqlite_connection.cpp
  db/db_path.cpp
  db/db_init_common.cpp
  db/db_resize.cpp
  db/db_init.cpp
  db/db_introspection.cpp
  domain/search_query.cpp
  domain/entry_operations.cpp
  store/entry_store.cpp
  repository/entry_repository.cpp
  repository/store_state_repository.cpp
  dao/karing_dao_common.cpp
  dao/karing_dao_read.cpp
  dao/karing_dao_write.cpp
)

target_link_libraries(karing_sqlite
  PUBLIC karing_project_options karing_common
  PUBLIC sqlite3
)

target_include_directories(karing_sqlite
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(karing_sqlite_file STATIC EXCLUDE_FROM_ALL
  dao/karing_dao_file.cpp
  domain/entry_operations_file.cpp
  store/entry_store_file.cpp
)

target_link_libraries(karing_sqlite_file
  PUBLIC karing_sqlite karing_file_storage
)
