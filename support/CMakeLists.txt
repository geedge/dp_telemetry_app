# CMakeFiles for 3rd library

include(ExternalProject)

#### cmocka
ExternalProject_Add(cmocka PREFIX cmocka
    URL ${CMAKE_CURRENT_SOURCE_DIR}/cmocka-1.1.7.tar.xz
    URL_MD5 dc23f4870eb7dfa6da065f3056e815b0
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

ExternalProject_Get_Property(cmocka INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(libcmocka STATIC IMPORTED GLOBAL)
add_dependencies(libcmocka cmocka)
set_target_properties(libcmocka PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib64/libcmocka.a
    INTERFACE_LINK_DIRECTORIES ${INSTALL_DIR}/lib64
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
)

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
set(COREI7 "corei7")
set(AARCH64 "aarch64")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(MACHINE ${COREI7})
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(MACHINE ${AARCH64})
else()
	message(FATAL_ERROR "${CMAKE_SYSTEM_PROCESSOR} processor is not currently supported")
endif()

if($ENV{CI} STREQUAL "true")
    set(CI_GITLAB_USER $ENV{CI_GITLAB_USER})
    set(CI_GITLAB_PASSWORD $ENV{CI_GITLAB_PASSWORD})
    set(GIT_REPOSITORY https://${CI_GITLAB_USER}:${CI_GITLAB_PASSWORD}@git.mesalab.cn/MESA_Platform/marsio.git)
    message(STATUS "Currently in the gitlab ci environment.")
else()
    set(GIT_REPOSITORY git@git.mesalab.cn:MESA_Platform/marsio.git)
endif()

ExternalProject_Add(marsio PREFIX marsio
            GIT_REPOSITORY ${GIT_REPOSITORY}
            GIT_TAG dev-4.8
            CONFIGURE_COMMAND 
                COMMAND mkdir -p build
                COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/build ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMACHINE=${MACHINE} ..
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
find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKA REQUIRED rdkafka)
# strange here
message(STATUS "RDKAFKA_INCLUDE_DIRS: ${RDKAFKA_INCLUDE_DIRS}")
message(STATUS "RDKAFKA_LIBRARY_DIRS: ${RDKAFKA_LIBRARY_DIRS}")
message(STATUS "RDKAFKA_LIBRARIES: ${RDKAFKA_LIBRARIES}")

##### zlog
ExternalProject_Add(zlog PREFIX zlog
    URL ${CMAKE_CURRENT_SOURCE_DIR}/zlog-1.2.17.tar.gz
    URL_MD5 d3ff9ba6cbafef89f98eb6a788ca2e78
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

ExternalProject_Get_Property(zlog INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(libzlog STATIC IMPORTED GLOBAL)
add_dependencies(libzlog zlog)
set_target_properties(libzlog PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libzlog.a
    INTERFACE_LINK_DIRECTORIES ${INSTALL_DIR}/lib
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    INTERFACE_LINK_LIBRARIES pthread
)

### cJSON
ExternalProject_Add(cJSON PREFIX cJSON
        URL ${CMAKE_CURRENT_SOURCE_DIR}/cJSON-1.7.7.tar.gz
        URL_MD5 715009c99728bf81d6c97352718650ff
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
				   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
				   -DBUILD_SHARED_AND_STATIC_LIBS=1)

ExternalProject_Get_Property(cJSON INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(libcjson SHARED IMPORTED GLOBAL)
add_dependencies(libcjson cJSON)
set_property(TARGET libcjson PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib64/libcjson.a)
set_property(TARGET libcjson PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)

### pcapng
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pcapng)