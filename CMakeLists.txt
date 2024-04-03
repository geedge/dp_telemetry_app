cmake_minimum_required(VERSION 3.0)

project(dp_trace_telemetry)

# envirment
message("CMake version: ${CMAKE_VERSION}")
message(STATUS "C Compiler Version: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

add_definitions(-D_GNU_SOURCE -D__STDC_LIMIT_MACROS)
# add_definitions(-DALLOW_EXPERIMENTAL_API=1) # for dpdk

include(ExternalProject)

# add dependencies: marsio
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "set build type to RelWithDebInfo")
endif()

ExternalProject_Add(marsio PREFIX marsio
            GIT_REPOSITORY git@git.mesalab.cn:MESA_Platform/marsio.git
            GIT_TAG dp_trace_telemetry
            CONFIGURE_COMMAND 
                COMMAND mkdir -p build
                COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/build ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..
            BUILD_COMMAND 
                COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/build make
            INSTALL_COMMAND ""
            BUILD_IN_SOURCE 1)

ExternalProject_Get_Property(marsio SOURCE_DIR)
file(MAKE_DIRECTORY ${SOURCE_DIR}/build/app)
file(MAKE_DIRECTORY ${SOURCE_DIR}/include/external)

add_library(libmarsio STATIC IMPORTED GLOBAL)
set_target_properties(libmarsio PROPERTIES
    IMPORTED_LOCATION ${SOURCE_DIR}/build/app/libmarsio.so
    INTERFACE_LINK_DIRECTORIES ${SOURCE_DIR}/build/app
    INTERFACE_INCLUDE_DIRECTORIES ${SOURCE_DIR}/include/external
)

# add dependencies: maat
set(MESA_FRAMEWORK_LIB_DIR /opt/MESA/lib)
set(MESA_FRAMEWORK_INCLUDE_DIR /opt/MESA/include)

add_library(maatframe SHARED IMPORTED GLOBAL)
set_property(TARGET maatframe PROPERTY IMPORTED_LOCATION ${MESA_FRAMEWORK_LIB_DIR}/libmaatframe.so)
set_property(TARGET maatframe PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MESA_FRAMEWORK_INCLUDE_DIR})

add_library(MESA_prof_load SHARED IMPORTED GLOBAL)
set_property(TARGET MESA_prof_load PROPERTY IMPORTED_LOCATION ${MESA_FRAMEWORK_LIB_DIR}/libMESA_prof_load.so)
set_property(TARGET MESA_prof_load PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MESA_FRAMEWORK_INCLUDE_DIR})

add_subdirectory(${CMAKE_SOURCE_DIR}/src)