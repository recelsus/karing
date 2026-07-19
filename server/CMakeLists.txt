add_library(karing_server_support STATIC
  utils/base_path.cpp
  utils/listen_probe.cpp
  utils/options.cpp
  utils/json_response.cpp
  utils/upload_mime.cpp
)

target_link_libraries(karing_server_support
  PUBLIC karing_project_options karing_common
  PRIVATE Drogon::Drogon
)

target_include_directories(karing_server_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Drogon controllers rely on translation-unit registration.
add_library(karing_server OBJECT
  init/cli_output.cpp
  init/bootstrap.cpp
  http/request_params.cpp
  http/record_json.cpp
  http/download_response.cpp
  services/root_service.cpp
  services/search_service.cpp
  controllers/karing_root_controller.cpp
  controllers/karing_search_controller.cpp
  controllers/karing_search_live_controller.cpp
  controllers/health_controller.cpp
)

target_link_libraries(karing_server
  PUBLIC karing_project_options karing_server_support karing_sqlite
  PRIVATE Drogon::Drogon
)

add_executable(karing-server
  main.cpp
  $<TARGET_OBJECTS:karing_server>
)

target_link_libraries(karing-server
  PRIVATE karing_project_options karing_server_support karing_sqlite Drogon::Drogon
)

install(TARGETS karing-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
