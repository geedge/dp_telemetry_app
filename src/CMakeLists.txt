include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/support/mpack)

set(DP_TELEMETRY_SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/config.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/job_ctx.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_output.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kafka.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mocking.c
    ${CMAKE_SOURCE_DIR}/support/mpack/mpack.c)

add_executable(${PROJECT_NAME} ${DP_TELEMETRY_SRC})
target_link_libraries(${PROJECT_NAME} libmarsio maatframe MESA_prof_load libevent-static rdkafka libcmocka libzlog uuid pthread)
if(ENABLE_DEVELOP_MOCKING)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--wrap=kafka_handle_create,--wrap=kafka_topic_new,--wrap=kafka_produce)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/conf
            ${CMAKE_BINARY_DIR}/conf
    COMMENT "copy config file to build directory"
)