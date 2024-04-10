cmake_minimum_required(VERSION 3.0)

project(dp_trace_telemetry)

# envirment
message("CMake version: ${CMAKE_VERSION}")
message(STATUS "C Compiler Version: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "set build type to RelWithDebInfo")
endif()

add_definitions(-D_GNU_SOURCE -D__STDC_LIMIT_MACROS)

option(ENABLE_DEVELOP_MOCKING "enable develop mocking" FALSE)

add_subdirectory(${CMAKE_SOURCE_DIR}/support)
add_subdirectory(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_SOURCE_DIR}/test)