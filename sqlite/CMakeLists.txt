add_library(karing_sqlite STATIC
  db/db_path.cpp
  db/db_init_common.cpp
  db/db_resize.cpp
  db/db_init.cpp
  db/db_introspection.cpp
  dao/karing_dao_common.cpp
  dao/karing_dao_read.cpp
  dao/karing_dao_write.cpp
)

target_link_libraries(karing_sqlite
  PUBLIC karing_project_options
  PRIVATE sqlite3
)
