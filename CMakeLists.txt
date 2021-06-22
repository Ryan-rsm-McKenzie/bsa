cmake_minimum_required(VERSION 3.20)

project(
	bsa_tes4
	LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
