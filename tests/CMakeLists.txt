if(TARGET karing_sqlite)
  add_subdirectory(sqlite)
endif()

if(TARGET karing_server)
  add_subdirectory(server)
endif()

if(TARGET karing_cli_core)
  add_subdirectory(cli)
endif()
