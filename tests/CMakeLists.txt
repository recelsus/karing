if(TARGET karing_sqlite)
  add_subdirectory(sqlite)
endif()

if(TARGET karing_server)
  add_subdirectory(server)
endif()
