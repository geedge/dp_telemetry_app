cmake_minimum_required(VERSION 3.0)

project(dp_telemetry)

include(ExternalProject)

# add dependencies: marsio
ExternalProject_Add(marsio PREFIX marsio
            GIT_REPOSITORY git@git.mesalab.cn:MESA_Platform/marsio.git
            GIT_TAG dp_trace_telemetry
            CONFIGURE_COMMAND 
                COMMAND mkdir -p build
                COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/build ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
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

add_subdirectory(${CMAKE_SOURCE_DIR}/src)