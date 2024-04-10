include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/support/mpack)

set(DP_TELEMETRY_SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/config.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/job_ctx.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_output.c
    ${CMAKE_SOURCE_DIR}/support/mpack/mpack.c)

add_executable(${PROJECT_NAME} ${DP_TELEMETRY_SRC})
target_link_libraries(${PROJECT_NAME} libmarsio maatframe MESA_prof_load libevent-static rdkafka uuid pthread)