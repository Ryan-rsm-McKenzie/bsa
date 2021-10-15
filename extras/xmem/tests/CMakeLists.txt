set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(SOURCE_DIR "${ROOT_DIR}/extras/xmem/tests")
set(SOURCE_FILES
	"${SOURCE_DIR}/src/bsa/xmem/api.test.cpp"
	"${SOURCE_DIR}/src/bsa/xmem/hashing.test.cpp"
	"${SOURCE_DIR}/src/bsa/xmem/unicode.test.cpp"
	"${SOURCE_DIR}/src/bsa/xmem/util.test.cpp"
	"${SOURCE_DIR}/src/bsa/xmem/winapi.test.cpp"
	"${SOURCE_DIR}/src/bsa/xmem/xmem.test.cpp"
	"${SOURCE_DIR}/src/main.test.cpp"
	"${SOURCE_DIR}/main.cpp"
)

source_group(TREE "${SOURCE_DIR}" PREFIX "src" FILES ${SOURCE_FILES})

add_executable(
	tests
	${SOURCE_FILES}
)
catch_discover_tests(tests)

target_compile_definitions(
	tests
	PRIVATE
		TESTING=1
)

target_include_directories(
	tests
	PRIVATE
		"${ROOT_DIR}/extras/xmem/src"
		"${SOURCE_DIR}"
)

target_link_libraries(
	tests
	PRIVATE
		"${PROJECT_NAME}_common::${PROJECT_NAME}_common"
		Catch2::Catch2
)

target_link_libraries(
	tests
	PRIVATE
		"${PROJECT_NAME}_common"
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"tests"
		PRIVATE
			"/utf-8"
	)
endif()

macro(acquire_test NAME)
	file(ARCHIVE_EXTRACT
		INPUT "${ROOT_DIR}/extras/xmem/data/${NAME}.7z"
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${NAME}"
	)
endmacro()

acquire_test("roundtrip_test")
