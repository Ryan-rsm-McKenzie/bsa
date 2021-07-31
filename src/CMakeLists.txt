set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(INCLUDE_DIR ${ROOT_DIR}/include)
set(HEADER_FILES
	${INCLUDE_DIR}/bsa/detail/common.hpp
	${INCLUDE_DIR}/bsa/bsa.hpp
	${INCLUDE_DIR}/bsa/fo4.hpp
	${INCLUDE_DIR}/bsa/fwd.hpp
	${INCLUDE_DIR}/bsa/tes3.hpp
	${INCLUDE_DIR}/bsa/tes4.hpp
)

set(SOURCE_DIR ${ROOT_DIR}/src)
set(SOURCE_FILES
	${SOURCE_DIR}/bsa/detail/common.cpp
	${SOURCE_DIR}/bsa/fo4.cpp
	${SOURCE_DIR}/bsa/tes3.cpp
	${SOURCE_DIR}/bsa/tes4.cpp
)

set(NATVIS_DIR ${ROOT_DIR}/visualizers)
set(NATVIS_FILES
	${NATVIS_DIR}/bsa_common.natvis
	${NATVIS_DIR}/bsa_fo4.natvis
	${NATVIS_DIR}/bsa_tes3.natvis
	${NATVIS_DIR}/bsa_tes4.natvis
)

source_group(
	TREE ${ROOT_DIR}
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
		${NATVIS_FILES}
)

set(TARGET_NAME bsa)
add_library(
	${TARGET_NAME}
	STATIC
	${HEADER_FILES}
	${SOURCE_FILES}
	${NATVIS_FILES}
	${ROOT_DIR}/.clang-format
	${ROOT_DIR}/.editorconfig
)
add_library(bsa::${TARGET_NAME} ALIAS ${TARGET_NAME})

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set_target_properties(
	${TARGET_NAME}
	PROPERTIES
		DEBUG_POSTFIX "d"
)

target_compile_features(
	${TARGET_NAME}
	PUBLIC
		cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_options(
		${TARGET_NAME}
		PRIVATE
			"-fvisibility=hidden"
	)
endif()

target_include_directories(
	${TARGET_NAME}
	PUBLIC
		"$<BUILD_INTERFACE:${INCLUDE_DIR}>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	PRIVATE
		${SOURCE_DIR}
)

list(APPEND CMAKE_MODULE_PATH "${ROOT_DIR}/cmake")

find_package(LZ4 MODULE REQUIRED)
find_package(ZLIB MODULE REQUIRED)

target_link_libraries(
	${TARGET_NAME}
	PRIVATE
		LZ4::LZ4
		ZLIB::ZLIB
)

install(
	TARGETS ${TARGET_NAME}
	EXPORT ${TARGET_NAME}-targets
)

install(
	EXPORT ${TARGET_NAME}-targets
	NAMESPACE bsa::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
)

configure_file(
	${ROOT_DIR}/cmake/config.cmake.in
	${TARGET_NAME}Config.cmake
	@ONLY
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake
	COMPATIBILITY SameMajorVersion
)

install(
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}Config.cmake
		${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake
		${ROOT_DIR}/cmake/FindLZ4.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
)

install(
	DIRECTORY ${INCLUDE_DIR}/bsa
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
	FILES ${NATVIS_FILES}
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${TARGET_NAME}
)
