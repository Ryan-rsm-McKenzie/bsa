set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(SOURCE_DIR ${ROOT_DIR}/tests)
set(SOURCE_FILES
	${SOURCE_DIR}/main.cpp
	${SOURCE_DIR}/tes3_tests.cpp
	${SOURCE_DIR}/tes4_tests.cpp
)
source_group("src" FILES ${SOURCE_FILES})

set(NATVIS_FILES
	${ROOT_DIR}/extern/CPPDebuggerVisualizers/VS2019/Visualizers/boost_Containers.natvis
)
source_group("visualizers" FILES ${NATVIS_FILES})

add_executable(
	tests
	${SOURCE_FILES}
	${NATVIS_FILES}
)
catch_discover_tests(tests)

target_compile_definitions(
	tests
	PRIVATE
		_CRT_SECURE_NO_WARNINGS
)

find_package(Boost
	MODULE
	REQUIRED
	COMPONENTS
		iostreams
		nowide
)

target_link_libraries(
	tests
	PRIVATE
		Boost::iostreams
		Boost::nowide
		bsa::bsa
		bsa::common
		Catch2::Catch2
)

function(download_test)
	set(_PREFIX download_file)
	set(ONE_VALUE_ARGS
		NAME
		SHA512
		TAG
	)

	cmake_parse_arguments(${_PREFIX} "" "${ONE_VALUE_ARGS}" "" ${ARGN})

	if(DEFINED "${_PREFIX}_KEYWORDS_MISSING_VALUES")
		foreach(ARG "${_PREFIX}_KEYWORDS_MISSING_VALUES")
			if(NOT DEFINED "${_PREFIX}_${ARG}")
				message(FATAL_ERROR "missing required argument: ${ARG}")
			endif()
		endforeach()
	endif()

	if(DEFINED "${_PREFIX}_UNPARSED_ARGUMENTS")
		foreach(ARG "${_PREFIX}_UNPARSED_ARGUMENTS")
			if(NOT DEFINED "${_PREFIX}_${ARG}")
				message(FATAL_ERROR "unused argument: ${ARG}")
			endif()
		endforeach()
	endif()

	set(NAME "${${_PREFIX}_NAME}")
	set(OUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.7z")

	file(DOWNLOAD
		"https://github.com/Ryan-rsm-McKenzie/bsa/releases/download/${${_PREFIX}_TAG}/${NAME}.7z"
		${OUT_FILE}
		EXPECTED_HASH SHA512=${${_PREFIX}_SHA512}
	)

	file(ARCHIVE_EXTRACT
		INPUT ${OUT_FILE}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${NAME}"
	)
endfunction()

download_test(
	NAME "compression_test"
	TAG "0.0.2"
	SHA512 "453816F36D499A6F2D97FEE4FEFB9784E2B021EA9F4443942E021877F3FA483643BFC8C7FA7B610A0B3207EED2C81B2515E63EE4CCC43AEC911C5D9AB585227C"
)

download_test(
	NAME "compression_mismatch_test"
	TAG "0.0.1"
	SHA512 "A8FAAE5C2244867E3A29A7E035D24F208FD8E08D7AAA727678340C0D6D23C75265DDC0A74D6F8AF26AA7099ADEB67E3944F5607F707204C19184277A276C166B"
)
