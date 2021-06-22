cmake_minimum_required(VERSION 3.20)

project(
	bsa_tes4
	LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

include(cmake/common.cmake)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
	find_package(Catch2 REQUIRED CONFIG)
	include(Catch)

	add_subdirectory(tests)
endif()
