cmake_minimum_required(VERSION 3.21)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
	bsa
	VERSION 2.0.1
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

option(BSA_BUILD_SRC "whether we should build the library itself" ON)
if("${BSA_BUILD_SRC}")
	add_subdirectory(src)
endif()

option(BSA_BUILD_EXAMPLES "whether we should build the examples" OFF)
if("${BSA_BUILD_EXAMPLES}")
	add_subdirectory(examples)
endif()

include(CTest)
if("${BUILD_TESTING}")
	find_package(Catch2 REQUIRED CONFIG)
	include(Catch)
	add_subdirectory(tests)
endif()

option(BSA_BUILD_DOCS "whether we should build documentation" OFF)
if("${BSA_BUILD_DOCS}")
	add_subdirectory(docs)
endif()
