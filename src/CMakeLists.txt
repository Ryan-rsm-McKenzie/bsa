set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(INCLUDE_DIR ${ROOT_DIR}/include)
set(HEADER_FILES
	${INCLUDE_DIR}/bsa/tes4.hpp
)

set(SOURCE_DIR ${ROOT_DIR}/src)
set(SOURCE_FILES
	${SOURCE_DIR}/bsa/tes4.cpp
)

source_group(
	TREE ${ROOT_DIR}/
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
)

set(NATVIS
	${ROOT_DIR}/extern/CPPDebuggerVisualizers/VS2019/Visualizers/boost_Containers.natvis
	${ROOT_DIR}/visualizers/bsa_tes4.natvis
)

source_group("visualizers" FILES ${NATVIS})

add_library(
	bsa
	STATIC
	${NATVIS}
	${HEADER_FILES}
	${SOURCE_FILES}
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

include(${ROOT_DIR}/cmake/common.cmake)

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
