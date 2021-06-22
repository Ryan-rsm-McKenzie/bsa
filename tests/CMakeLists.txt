set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(tests main.cpp)

find_package(Catch2 REQUIRED CONFIG)

include(${ROOT_DIR}/cmake/common.cmake)

target_link_libraries(
	tests
	PUBLIC
		Catch2::Catch2
		common::common
)
