set(DP_TRACE_TELEMETRY_SOURCES 
    ${CMAKE_SOURCE_DIR}/src/config.c 
    ${CMAKE_SOURCE_DIR}/src/job_ctx.c 
    ${CMAKE_SOURCE_DIR}/src/trace_output.c
    ${CMAKE_SOURCE_DIR}/src/kafka.c
    ${CMAKE_SOURCE_DIR}/src/maat.c
    ${CMAKE_SOURCE_DIR}/src/monit.c
    ${CMAKE_SOURCE_DIR}/src/http_serv.c
    ${CMAKE_SOURCE_DIR}/src/logger.c
    ${CMAKE_SOURCE_DIR}/support/mpack/mpack.c)

add_executable(cmocka_test cmocka_test.c  ${DP_TRACE_TELEMETRY_SOURCES})

target_include_directories(cmocka_test PRIVATE ${CMAKE_SOURCE_DIR}/support/mpack ${RDKAFKA_INCLUDE_DIRS})
target_include_directories(cmocka_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_directories(cmocka_test PRIVATE ${RDKAFKA_LIBRARY_DIRS})

target_link_libraries(cmocka_test libpcapng libmarsio maatframe MESA_prof_load libevent-static ${RDKAFKA_LIBRARIES} libcmocka libzlog libcjson uuid pthread pcap)
