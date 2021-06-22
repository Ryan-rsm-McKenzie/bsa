set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(tests main.cpp)
catch_discover_tests(tests)

target_link_libraries(
	tests
	PUBLIC
		bsa::bsa
		Catch2::Catch2
		common::common
)
