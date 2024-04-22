cmake_minimum_required(VERSION 3.0)

project(pcapng)

add_library(libpcapng STATIC
    pcapng.c
)

target_include_directories(libpcapng PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(libpcapng PRIVATE libmarsio)