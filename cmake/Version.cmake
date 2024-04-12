# Using autorevision.sh to generate version information

set(__SOURCE_AUTORESIVISION ${CMAKE_SOURCE_DIR}/autorevision.sh)
set(__AUTORESIVISION ${CMAKE_BINARY_DIR}/autorevision.sh)
set(__VERSION_CACHE ${CMAKE_BINARY_DIR}/version.txt)
set(__VERSION_CONFIG ${CMAKE_BINARY_DIR}/version.cmake)
set(__VERSION_CONFIG_C_IN ${CMAKE_SOURCE_DIR}/template/dp_trace_version.h.in)
set(__VERSION_CONFIG_C_OUT ${CMAKE_BINARY_DIR}/include/dp_trace_version.h)

file(COPY ${__SOURCE_AUTORESIVISION} DESTINATION ${CMAKE_BINARY_DIR}
	FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
	WORLD_READ WORLD_EXECUTE)

# execute autorevision.sh to generate version information
execute_process(COMMAND ${__AUTORESIVISION} -t cmake -o ${__VERSION_CACHE} 
				OUTPUT_FILE ${__VERSION_CONFIG} ERROR_QUIET)
include(${__VERSION_CONFIG})

set(DP_TRACE_COMMIT_HASH "${VCS_SHORT_HASH}")

# extract major, minor, patch version from git tag
string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" DP_TRACE_VERSION_MAJOR "${VCS_TAG}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" DP_TRACE_VERSION_MINOR "${VCS_TAG}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" DP_TRACE_VERSION_PATCH "${VCS_TAG}")

if(NOT DP_TRACE_VERSION_MAJOR)
	set(DP_TRACE_VERSION_MAJOR 0)
endif()

if(NOT DP_TRACE_VERSION_MINOR)
	set(DP_TRACE_VERSION_MINOR 1)
endif()

if(NOT DP_TRACE_VERSION_PATCH)
	set(DP_TRACE_VERSION_PATCH 0)
endif()

set(DP_TRACE_VERSION "${DP_TRACE_VERSION_MAJOR}.${DP_TRACE_VERSION_MINOR}.${DP_TRACE_VERSION_PATCH}")

# system information
execute_process(COMMAND uname -r OUTPUT_VARIABLE DP_TRACE_VERSION_KERNEL)

# generate c header file
configure_file(${__VERSION_CONFIG_C_IN} ${__VERSION_CONFIG_C_OUT} @ONLY)
add_definitions(-include ${__VERSION_CONFIG_C_OUT})

# print information
message(STATUS "DP trace Version: ${DP_TRACE_VERSION}")
message(STATUS "Kernel Version: ${DP_TRACE_VERSION_KERNEL}")