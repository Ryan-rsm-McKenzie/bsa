set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(SOURCE_DIR "${ROOT_DIR}/extras/xmem/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/bsa/xmem/api.hpp"
	"${SOURCE_DIR}/bsa/xmem/hashing.cpp"
	"${SOURCE_DIR}/bsa/xmem/hashing.hpp"
	"${SOURCE_DIR}/bsa/xmem/util.hpp"
	"${SOURCE_DIR}/bsa/xmem/winapi.hpp"
	"${SOURCE_DIR}/bsa/xmem/xcompress.cpp"
	"${SOURCE_DIR}/bsa/xmem/xcompress.hpp"
	"${SOURCE_DIR}/bsa/xmem/xmem.hpp"
)

source_group(TREE "${SOURCE_DIR}" PREFIX "src" FILES ${SOURCE_FILES})

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/bsa/xmem/version.hpp")
configure_file(
	"${ROOT_DIR}/extras/xmem/cmake/version.hpp.in"
	"${VERSION_HEADER}"
	@ONLY
)

source_group("src/bsa/xmem" FILES "${VERSION_HEADER}")

# ---- support library ----

add_library(
	"${PROJECT_NAME}_common"
	STATIC
	"${VERSION_HEADER}"
	${SOURCE_FILES}
)
add_library("${PROJECT_NAME}_common::${PROJECT_NAME}_common" ALIAS "${PROJECT_NAME}_common")

target_compile_features(
	"${PROJECT_NAME}_common"
	PUBLIC
		cxx_std_20
)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
	target_compile_options(
		"${PROJECT_NAME}_common"
		PUBLIC
			"-fvisibility=hidden"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}_common"
	PUBLIC
		"${CMAKE_CURRENT_BINARY_DIR}/src"
		"${SOURCE_DIR}"
)

find_package(binary_io REQUIRED CONFIG)
find_package(xbyak REQUIRED CONFIG)
find_package(mmio REQUIRED CONFIG)
find_package(reproc++ REQUIRED CONFIG)
find_package(tl-expected REQUIRED CONFIG)

target_link_libraries(
	"${PROJECT_NAME}_common"
	PUBLIC
		Bcrypt.lib
		binary_io::binary_io
		xbyak::xbyak
		mmio::mmio
		reproc++
		tl::expected
)

# ---- executable ----

configure_file(
	"${ROOT_DIR}/extras/xmem/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

source_group("res" FILES "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
source_group("src" FILES "${SOURCE_DIR}/main.cpp")

add_executable(
	"${PROJECT_NAME}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${SOURCE_DIR}/main.cpp"
)

find_package(args REQUIRED CONFIG)
find_package(mmio REQUIRED CONFIG)

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		"${PROJECT_NAME}_common::${PROJECT_NAME}_common"
		mmio::mmio
		taywee::args
)
