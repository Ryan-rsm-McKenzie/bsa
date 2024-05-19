set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(SOURCE_DIR "${ROOT_DIR}/tests")
set(SOURCE_FILES
	"${SOURCE_DIR}/src/bsa/detail/common.test.cpp"
	"${SOURCE_DIR}/src/bsa/fo4.test.cpp"
	"${SOURCE_DIR}/src/bsa/tes3.test.cpp"
	"${SOURCE_DIR}/src/bsa/tes4.test.cpp"
	"${SOURCE_DIR}/catch2.hpp"
	"${SOURCE_DIR}/utility.hpp"
)

if("${BSA_BUILD_EXAMPLES}")
	list(APPEND SOURCE_FILES
		"${SOURCE_DIR}/examples/pack_unpack.test.cpp"
	)
endif()

source_group(TREE "${SOURCE_DIR}" PREFIX "src" FILES ${SOURCE_FILES})

add_executable(
	tests
	${SOURCE_FILES}
)
catch_discover_tests(tests)

include(GNUInstallDirs)

target_compile_definitions(
	tests
	PRIVATE
		_CRT_SECURE_NO_WARNINGS
		TESTING=1
)

target_include_directories(
	tests
	PRIVATE
		"${SOURCE_DIR}"
)

target_link_libraries(
	tests
	PRIVATE
		"${PROJECT_NAME}::${PROJECT_NAME}"
		Catch2::Catch2WithMain
)

macro(acquire_test NAME)
	file(ARCHIVE_EXTRACT
		INPUT "${ROOT_DIR}/data/${NAME}.7z"
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${NAME}"
	)
endmacro()

acquire_test("common_guess_test")

acquire_test("fo4_chunk_test")
acquire_test("fo4_compression_test")
acquire_test("fo4_cubemap_test")
acquire_test("fo4_dds_test")
acquire_test("fo4_dx9_test")
acquire_test("fo4_invalid_test")
acquire_test("fo4_missing_string_table_test")
acquire_test("fo4_next_gen_test")
acquire_test("fo4_write_test")

acquire_test("tes3_invalid_test")
acquire_test("tes3_read_test")
acquire_test("tes3_write_test")

acquire_test("tes4_compression_mismatch_test")
acquire_test("tes4_compression_test")
acquire_test("tes4_data_sharing_name_test")
acquire_test("tes4_flags_test")
acquire_test("tes4_invalid_test")
acquire_test("tes4_xbox_read_test")
acquire_test("tes4_xbox_write_test")
acquire_test("tes4_xmem_test")

acquire_test("in_memory_test")

if("${BSA_BUILD_EXAMPLES}")
	target_include_directories(
		tests
		PRIVATE
			"${ROOT_DIR}/examples"
	)

	acquire_test("examples_pack_unpack_test")
endif()

if("${BSA_SUPPORT_XMEM}")
	find_program(
		XMEM_EXE
		NAMES "xmem.exe"
		HINTS
			"${ROOT_DIR}/installed"
		PATH_SUFFIXES
			"${CMAKE_INSTALL_BINDIR}"
		REQUIRED
	)

	find_file(
		XMEM_PDB
		NAMES "xmem.pdb"
		HINTS
			"${ROOT_DIR}/installed"
		PATH_SUFFIXES
			"${CMAKE_INSTALL_BINDIR}"
		REQUIRED
	)

	add_custom_command(
		TARGET tests
		PRE_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${XMEM_EXE}" "$<TARGET_FILE_DIR:tests>"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${XMEM_PDB}" "$<TARGET_FILE_DIR:tests>"
	)
endif()
