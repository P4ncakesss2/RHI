add_executable(Project
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
)

target_link_libraries(Project PRIVATE engine)