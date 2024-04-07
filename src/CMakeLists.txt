include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} main.c config.c job_ctx.c trace_output.c)
target_link_libraries(${PROJECT_NAME} libmarsio maatframe MESA_prof_load libevent-static uuid pthread)