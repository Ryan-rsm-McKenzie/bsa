set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(INCLUDE_DIR ${ROOT_DIR}/tests)
set(HEADER_FILES
	${INCLUDE_DIR}/utility.hpp
)
source_group("include" FILES ${HEADER_FILES})

set(SOURCE_DIR ${ROOT_DIR}/tests)
set(SOURCE_FILES
	${SOURCE_DIR}/detail/common.test.cpp
	${SOURCE_DIR}/fo4.test.cpp
	${SOURCE_DIR}/main.cpp
	${SOURCE_DIR}/tes3.test.cpp
	${SOURCE_DIR}/tes4.test.cpp
)
source_group(TREE ${SOURCE_DIR} PREFIX "src" FILES ${SOURCE_FILES})

set(NATVIS_DIR ${ROOT_DIR}/external/CPPDebuggerVisualizers/VS2019/Visualizers)
set(NATVIS_FILES
	${NATVIS_DIR}/boost.natvis
	${NATVIS_DIR}/boost_Containers.natvis
	${NATVIS_DIR}/boost_Regex.natvis
)
source_group("visualizers" FILES ${NATVIS_FILES})

add_executable(
	tests
	${HEADER_FILES}
	${SOURCE_FILES}
	${NATVIS_FILES}
)
catch_discover_tests(tests)

target_compile_definitions(
	tests
	PRIVATE
		_CRT_SECURE_NO_WARNINGS
)

target_include_directories(
	tests
	PRIVATE
		${INCLUDE_DIR}
		${SOURCE_DIR}
)

find_package(Boost
	MODULE
	REQUIRED
	COMPONENTS
		regex
)

target_link_libraries(
	tests
	PRIVATE
		Boost::regex
		bsa::bsa
		bsa::common
		Catch2::Catch2
)

macro(acquire_test NAME)
	file(ARCHIVE_EXTRACT
		INPUT "${ROOT_DIR}/data/${NAME}.7z"
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${NAME}"
	)
endmacro()

acquire_test("fo4_compression_test")
acquire_test("fo4_dds_test")
acquire_test("fo4_invalid_test")
acquire_test("fo4_missing_string_table_test")
acquire_test("fo4_write_test")

acquire_test("tes3_invalid_test")
acquire_test("tes3_read_test")
acquire_test("tes3_write_test")

acquire_test("tes4_compression_mismatch_test")
acquire_test("tes4_compression_test")
acquire_test("tes4_flags_test")
acquire_test("tes4_invalid_test")
acquire_test("tes4_xbox_read_test")
acquire_test("tes4_xbox_write_test")
