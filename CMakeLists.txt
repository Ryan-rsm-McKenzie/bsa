cmake_minimum_required(VERSION 3.20)

project(
	bsa
	VERSION 1.0.0
	LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

include(cmake/common.cmake)

option(BSA_BUILD_SRC "whether we should build the library itself" ON)
if(BSA_BUILD_SRC)
	add_subdirectory(src)
endif()

include(CTest)
if(BUILD_TESTING)
	find_package(Catch2 REQUIRED CONFIG)
	include(Catch)

	add_subdirectory(tests)
endif()

option(BSA_BUILD_DOCS "whether we should build documentation" OFF)
if(BSA_BUILD_DOCS)
	add_subdirectory(docs)
endif()
