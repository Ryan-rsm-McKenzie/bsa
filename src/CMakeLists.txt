set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(INCLUDE_DIR ${ROOT_DIR}/include)
set(HEADER_FILES
	${INCLUDE_DIR}/bsa/tes4.hpp
)

set(SOURCE_DIR ${ROOT_DIR}/src)
set(SOURCE_FILES
	${SOURCE_DIR}/bsa/tes4.cpp
)

set(NATVIS_DIR ${ROOT_DIR}/visualizers)
set(NATVIS_FILES
	${NATVIS_DIR}/bsa_tes4.natvis
)

source_group(
	TREE ${ROOT_DIR}
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
		${NATVIS_FILES}
)

add_library(
	bsa
	STATIC
	${HEADER_FILES}
	${SOURCE_FILES}
	${NATVIS_FILES}
	${ROOT_DIR}/.clang-format
	${ROOT_DIR}/.editorconfig
)
add_library(bsa::bsa ALIAS bsa)

include(GNUInstallDirs)

target_include_directories(
	bsa
	PUBLIC
		"$<BUILD_INTERFACE:${INCLUDE_DIR}>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	PRIVATE
		${SOURCE_DIR}
)

find_package(Boost
	MODULE
	REQUIRED
	COMPONENTS
		container
		filesystem
		iostreams
)
find_package(text REQUIRED CONFIG)

target_link_libraries(
	bsa
	PRIVATE
		Boost::text
	PUBLIC
		Boost::container
		Boost::filesystem
		Boost::headers
		Boost::iostreams
		common::common
)
