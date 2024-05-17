include_directories(${CMAKE_SOURCE_DIR}/support/mpack)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(DP_TRACE_TELEMETRY_SOURCES 
    ${CMAKE_SOURCE_DIR}/src/config.c 
    ${CMAKE_SOURCE_DIR}/src/job_ctx.c 
    ${CMAKE_SOURCE_DIR}/src/trace_output.c
    ${CMAKE_SOURCE_DIR}/src/kafka.c
    ${CMAKE_SOURCE_DIR}/src/maat.c
    ${CMAKE_SOURCE_DIR}/src/mocking.c
    ${CMAKE_SOURCE_DIR}/src/monit.c
    ${CMAKE_SOURCE_DIR}/src/http_serv.c
    ${CMAKE_SOURCE_DIR}/src/logger.c
    ${CMAKE_SOURCE_DIR}/support/mpack/mpack.c)

add_executable(cmocka_test cmocka_test.c  ${DP_TRACE_TELEMETRY_SOURCES})
target_link_libraries(cmocka_test libpcapng libmarsio maatframe MESA_prof_load libevent-static rdkafka libcmocka libzlog libcjson uuid pthread pcap)
