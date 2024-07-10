set(DP_TELEMETRY_SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/config.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/job_ctx.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_output.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kafka.c
    ${CMAKE_CURRENT_SOURCE_DIR}/maat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/monit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/http_serv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.c
    ${CMAKE_SOURCE_DIR}/support/mpack/mpack.c)

add_executable(${PROJECT_NAME} ${DP_TELEMETRY_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/support/mpack ${RDKAFKA_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_directories(${PROJECT_NAME} PRIVATE ${RDKAFKA_LIBRARY_DIRS})

target_link_libraries(${PROJECT_NAME} libpcapng libmarsio maatframe MESA_prof_load libevent-static ${RDKAFKA_LIBRARIES} libcmocka libzlog libcjson uuid pthread pcap)

# if(ENABLE_DEVELOP_MOCKING)
#    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--wrap=kafka_handle_create,--wrap=kafka_topic_new,--wrap=kafka_produce)
# endif()

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#             ${CMAKE_SOURCE_DIR}/etc
#             ${CMAKE_BINARY_DIR}/etc
#     COMMENT "copy config file to build directory"
# )

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALL_BIN_DIR} COMPONENT Program)

file(GLOB CONFIG_FILES ${CMAKE_SOURCE_DIR}/etc/*)
install(FILES ${CONFIG_FILES} DESTINATION ${INSTALL_ETC_DIR} COMPONENT Profile)