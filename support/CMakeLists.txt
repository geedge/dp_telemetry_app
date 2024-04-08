# CMakeFiles for 3rd library

include(ExternalProject)

#### GoogleTest
ExternalProject_Add(googletest PREFIX googletest
    URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.0.tar.gz
    URL_MD5 16877098823401d1bf2ed7891d7dce36
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

ExternalProject_Get_Property(googletest INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(gtest STATIC IMPORTED GLOBAL)
add_dependencies(gtest googletest)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtest.a)
set_property(TARGET gtest PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
set_property(TARGET gtest PROPERTY INTERFACE_LINK_LIBRARIES pthread)

add_library(gtest_main STATIC IMPORTED GLOBAL)
add_dependencies(gtest_main googletest)
set_property(TARGET gtest_main PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtest_main.a)
set_property(TARGET gtest_main PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
set_property(TARGET gtest_main PROPERTY INTERFACE_LINK_LIBRARIES pthread gtest)

add_library(gmock STATIC IMPORTED GLOBAL)
add_dependencies(gmock googletest)
set_property(TARGET gmock PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgmock.a)
set_property(TARGET gmock PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)

add_library(gmock_main STATIC IMPORTED GLOBAL)
add_dependencies(gmock_main googletest)
set_property(TARGET gmock_main PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgmock_main.a)
set_property(TARGET gmock_main PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)

#### libevent
ExternalProject_Add(libevent PREFIX libevent
        URL ${CMAKE_CURRENT_SOURCE_DIR}/libevent-2.1.12-stable.tar.bz2
        URL_MD5 977fb199eb49d46e2c3a924143e5f570
        CONFIGURE_COMMAND ./configure --prefix=<INSTALL_DIR> --disable-shared --disable-samples --disable-openssl
        BUILD_COMMAND make LDFLAGS="-ldl"
        BUILD_IN_SOURCE 1)

ExternalProject_Get_Property(libevent INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(libevent-static STATIC IMPORTED GLOBAL)
add_dependencies(libevent-static libevent)
set_property(TARGET libevent-static PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libevent.a)
set_property(TARGET libevent-static PROPERTY IMPORTED_INTERFACE_LINK_LIBRARIES pthread crypto)
set_property(TARGET libevent-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)

add_library(libevent-static-pthreads STATIC IMPORTED GLOBAL)
add_dependencies(libevent-static-pthreads libevent)
set_property(TARGET libevent-static-pthreads PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libevent_pthreads.a)
set_property(TARGET libevent-static-pthreads PROPERTY IMPORTED_INTERFACE_LINK_LIBRARIES pthread crypto)
set_property(TARGET libevent-static-pthreads PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)


##### marsio
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

##### maat
set(MESA_FRAMEWORK_LIB_DIR /opt/MESA/lib)
set(MESA_FRAMEWORK_INCLUDE_DIR /opt/MESA/include)

add_library(maatframe SHARED IMPORTED GLOBAL)
set_property(TARGET maatframe PROPERTY IMPORTED_LOCATION ${MESA_FRAMEWORK_LIB_DIR}/libmaatframe.so)
set_property(TARGET maatframe PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MESA_FRAMEWORK_INCLUDE_DIR})

##### MESA_prof_load
add_library(MESA_prof_load SHARED IMPORTED GLOBAL)
set_property(TARGET MESA_prof_load PROPERTY IMPORTED_LOCATION ${MESA_FRAMEWORK_LIB_DIR}/libMESA_prof_load.so)
set_property(TARGET MESA_prof_load PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MESA_FRAMEWORK_INCLUDE_DIR})

##### kafka
add_library(rdkafka SHARED IMPORTED GLOBAL)
set_property(TARGET rdkafka PROPERTY IMPORTED_LOCATION ${MESA_FRAMEWORK_LIB_DIR}/librdkafka.so)
set_property(TARGET rdkafka PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MESA_FRAMEWORK_INCLUDE_DIR}/MESA)