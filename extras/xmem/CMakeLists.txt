cmake_minimum_required(VERSION 3.21)

project(
	xmem
	LANGUAGES CXX
)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

option(XMEM_BUILD_SRC "whether we should build the exe itself" ON)
if("${XMEM_BUILD_SRC}")
	add_subdirectory(src)
endif()

include(CTest)
if("${BUILD_TESTING}")
	find_package(Catch2 REQUIRED CONFIG)
	include(Catch)
	add_subdirectory(tests)
endif()
