set(TARGET LZ4)

find_package(PkgConfig QUIET MODULE)
if("${PkgConfig_FOUND}")
	pkg_check_modules("PC_${TARGET}" QUIET liblz4)
endif()

find_path("${TARGET}_INCLUDE_DIR"
	NAMES
		"lz4.h"
		"lz4frame.h"
		"lz4hc.h"
	PATHS ${PC_${TARGET}_INCLUDE_DIRS}
	PATH_SUFFIXES "${TARGET}"
)

find_library("${TARGET}_LIBRARY_RELEASE"
	NAMES "lz4"
	PATHS ${PC_${TARGET}_LIBRARY_DIRS}
)
find_library("${TARGET}_LIBRARY_DEBUG"
	NAMES "lz4d"
	PATHS ${PC_${TARGET}_LIBRARY_DIRS}
)

include(SelectLibraryConfigurations)
select_library_configurations("${TARGET}")

set("${TARGET}_VERSION" "${PC_${TARGET}_VERSION}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args("${TARGET}"
	FOUND_VAR "${TARGET}_FOUND"
	REQUIRED_VARS
		"${TARGET}_LIBRARY"
		"${TARGET}_INCLUDE_DIR"
	VERSION_VAR "${TARGET}_VERSION"
)

if("${${TARGET}_FOUND}")
	set("${TARGET}_LIBRARIES" ${${TARGET}_LIBRARY})
	set("${TARGET}_INCLUDE_DIRS" ${${TARGET}_INCLUDE_DIR})
	set("${TARGET}_DEFINITIONS" ${PC_${TARGET}_CFLAGS_OTHER})

	if(NOT TARGET "${TARGET}::${TARGET}")
		add_library("${TARGET}::${TARGET}" UNKNOWN IMPORTED)
	endif()

	if(${TARGET}_LIBRARY_RELEASE)
		set_property(
			TARGET "${TARGET}::${TARGET}"
			APPEND
			PROPERTY IMPORTED_CONFIGURATIONS RELEASE
		)
		set_target_properties(
			"${TARGET}::${TARGET}"
			PROPERTIES
				IMPORTED_LOCATION_RELEASE "${${TARGET}_LIBRARY_RELEASE}"
		)
	endif()

	if(${TARGET}_LIBRARY_DEBUG)
		set_property(
			TARGET "${TARGET}::${TARGET}"
			APPEND
			PROPERTY IMPORTED_CONFIGURATIONS DEBUG
		)
		set_target_properties(
			"${TARGET}::${TARGET}"
			PROPERTIES
				IMPORTED_LOCATION_DEBUG "${${TARGET}_LIBRARY_DEBUG}"
		)
	endif()

	if(NOT ${TARGET}_LIBRARY_RELEASE AND NOT ${TARGET}_LIBRARY_DEBUG)
		set_property(
			TARGET "${TARGET}::${TARGET}"
			APPEND
			PROPERTY IMPORTED_LOCATION "${${TARGET}_LIBRARY}"
		)
	endif()

	set_target_properties(
		"${TARGET}::${TARGET}"
		PROPERTIES
			INTERFACE_COMPILE_OPTIONS "${${TARGET}_DEFINITIONS}"
			INTERFACE_INCLUDE_DIRECTORIES "${${TARGET}_INCLUDE_DIRS}"
	)
endif()
