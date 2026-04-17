add_library(karing_core STATIC
  utils/options.cpp
  utils/json_response.cpp
  utils/search_query.cpp
  utils/upload_mime.cpp
)

target_link_libraries(karing_core
  PUBLIC karing_project_options
  PRIVATE Drogon::Drogon sqlite3
)

# Drogon controllers rely on translation-unit registration.
add_library(karing_server OBJECT
  init/cli_output.cpp
  init/bootstrap.cpp
  http/request_params.cpp
  http/record_json.cpp
  http/download_response.cpp
  services/root_service.cpp
  controllers/karing_root_controller.cpp
  controllers/karing_search_controller.cpp
  controllers/karing_search_live_controller.cpp
  controllers/health_controller.cpp
)

target_link_libraries(karing_server
  PUBLIC karing_project_options karing_core karing_sqlite
  PRIVATE Drogon::Drogon sqlite3
)

add_executable(karing-server
  main.cpp
  $<TARGET_OBJECTS:karing_server>
)

target_link_libraries(karing-server
  PRIVATE karing_project_options karing_core karing_sqlite Drogon::Drogon sqlite3
)

install(TARGETS karing-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
