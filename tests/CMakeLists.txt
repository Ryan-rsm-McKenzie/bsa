set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(SOURCE_DIR ${ROOT_DIR}/tests)
set(SOURCE_FILES
	${SOURCE_DIR}/main.cpp
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
	PUBLIC
		_CRT_SECURE_NO_WARNINGS
)

target_link_libraries(
	tests
	PUBLIC
		bsa::bsa
		Catch2::Catch2
		bsa::common
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
	TAG "0.0.1"
	SHA512 "4EA178D08F0EE91A375AA31077C9C23FB12CB06F1856D8A187D0CD17034D2C04001D8CDABD438C18A9D4FD51FE56E988DBE0827DE181134275B23F50B376645E"
)
